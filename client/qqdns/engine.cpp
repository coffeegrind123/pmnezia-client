// SPDX-License-Identifier: GPL-3.0-or-later

#include "engine.h"

#include "codec.h"
#include "dnsframing.h"
#include "reassembly.h"

#include <QElapsedTimer>
#include <QHostAddress>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QQueue>
#include <QRandomGenerator>
#include <QTimer>
#include <QUdpSocket>
#include <QVector>

#include <memory>

Q_LOGGING_CATEGORY(qqdnsEngine, "qqdns.engine")

namespace amnezia::qqdns {

namespace {
constexpr qint64 kAssembleMs = 13000;   // ASSEMBLE_TIME
constexpr int kDefaultSendSocks = 16;   // conservative for a GUI client
constexpr int kMaxSendSocks = 1024;

// Parse a "host" (defaults to :53) or "host:port" resolver spec. IPv6 must use
// the [addr]:port form to carry a port (matches the reference's practical
// scope). Returns false on an unparseable address.
bool parseResolver(const QString &spec, QHostAddress &outAddr, quint16 &outPort)
{
    QString host = spec.trimmed();
    quint16 port = 53;
    const int colon = host.lastIndexOf(':');
    if (colon > 0 && host.indexOf(':') == colon) { // exactly one ':' => host:port
        bool ok = false;
        const quint16 p = host.mid(colon + 1).toUShort(&ok);
        if (ok) {
            port = p;
            host = host.left(colon);
        }
    } else if (host.startsWith('[')) { // [v6]:port
        const int close = host.indexOf(']');
        if (close > 0) {
            bool ok = false;
            if (close + 1 < host.size() && host[close + 1] == ':') {
                const quint16 p = host.mid(close + 2).toUShort(&ok);
                if (ok) {
                    port = p;
                }
            }
            host = host.mid(1, close - 1);
        }
    }
    QHostAddress addr(host);
    if (addr.isNull()) {
        return false;
    }
    outAddr = addr;
    outPort = port;
    return true;
}

QList<QByteArray> jsonStringList(const QJsonObject &obj, const char *key)
{
    QList<QByteArray> out;
    for (const QJsonValue &v : obj.value(QLatin1String(key)).toArray()) {
        const QString s = v.toString().trimmed();
        if (!s.isEmpty()) {
            out.append(s.toLatin1());
        }
    }
    return out;
}
} // namespace

// ---------------------------------------------------------------------------

class EnginePrivate : public QObject
{
    Q_OBJECT
public:
    explicit EnginePrivate(QObject *parent) : QObject(parent) {}

    struct PendingSend {
        int sockIndex;
        QByteArray query;
        qint64 entryMs;
    };
    struct Resolver {
        QHostAddress addr;
        quint16 port = 53;
        QQueue<PendingSend> queue;
        QTimer *timer = nullptr;
    };

    Engine::State state = Engine::State::Idle;
    QString lastError;

    QUdpSocket *hIn = nullptr;
    QUdpSocket *wan = nullptr;
    QVector<QUdpSocket *> sendSocks;
    std::vector<std::unique_ptr<Resolver>> resolvers;

    QList<SendDomain> sendDomains;
    QList<QList<QByteArray>> recvDomains;
    std::unique_ptr<DataHandler> reasm;
    QTimer *sweepTimer = nullptr;

    // Wire-shape config.
    int maxSubLen = 63;
    int maxDomainLen = 253;
    int tries = 2; // retries + 1
    quint16 sendQueryType = 1;
    qint64 sendIntervalMs = 1;
    qint64 waitLimitMs = 1000;

    // Rolling indices (single-thread; no sync needed).
    int sendSockIndex = 0;
    quint16 queryId = 0;
    quint32 dataOffset = 0;
    int resIndex = 0;
    int sendDomainIndex = 0;

    // Learned local-app (AmneziaWG) address — client role.
    QHostAddress lastHAddr;
    quint16 lastHPort = 0;
    bool lastHValid = false;

    QElapsedTimer clock;
    quint64 bytesRecv = 0;
    quint64 bytesSent = 0;

    void setState(Engine::State s)
    {
        if (state != s) {
            state = s;
            emit stateChanged(s);
        }
    }

    void fail(const QString &msg)
    {
        qCWarning(qqdnsEngine) << "qqdns engine failed:" << msg;
        lastError = msg;
        teardown();
        setState(Engine::State::Failed);
    }

    qint64 nowMs() const { return clock.isValid() ? clock.elapsed() : 0; }

    bool startEngine(const QJsonObject &config);
    void teardown();

    void onHInReadyRead();
    void onWanReadyRead();
    void onResolverTimer(Resolver *r);

signals:
    void stateChanged(amnezia::qqdns::Engine::State newState);
    void bytesChanged(quint64 receivedDelta, quint64 sentDelta);
};

bool EnginePrivate::startEngine(const QJsonObject &config)
{
    clock.start();
    setState(Engine::State::Starting);

    // --- parse config ---
    const QList<QByteArray> dnsIps = jsonStringList(config, "dns_ips");
    const QList<QByteArray> sendDoms = jsonStringList(config, "send_domains");
    const QList<QByteArray> recvDoms = jsonStringList(config, "recv_domains");
    if (dnsIps.isEmpty()) {
        fail("dns_ips is empty");
        return false;
    }
    if (sendDoms.isEmpty()) {
        fail("send_domains is empty");
        return false;
    }
    if (recvDoms.isEmpty()) {
        fail("recv_domains is empty");
        return false;
    }

    maxDomainLen = config.value("max_domain_len").toInt(253);
    maxSubLen = config.value("max_sub_len").toInt(63);
    if (maxDomainLen + 2 > 255 || maxSubLen > 63 || maxSubLen < 1) {
        fail("max_domain_len/max_sub_len out of range");
        return false;
    }
    tries = qMax(0, config.value("retries").toInt(1)) + 1;
    sendQueryType = static_cast<quint16>(config.value("send_query_type").toInt(1));
    sendIntervalMs = qMax<qint64>(0, config.value("packets_send_interval_ms").toInt(1));
    waitLimitMs = qMax<qint64>(1, config.value("packets_wait_time_limit_ms").toInt(1000));
    int sendSockCount = config.value("send_sock_numbers").toInt(kDefaultSendSocks);
    sendSockCount = qBound(1, sendSockCount, kMaxSendSocks);

    const QString sendIfaceIp = [&]() {
        const QString s = config.value("send_interface_ip").toString().trimmed();
        return s.isEmpty() ? QStringLiteral("0.0.0.0") : s;
    }();
    const QString recvIfaceIp = [&]() {
        const QString s = config.value("receive_interface_ip").toString().trimmed();
        return s.isEmpty() ? QStringLiteral("0.0.0.0") : s;
    }();
    const quint16 recvPort = static_cast<quint16>(config.value("receive_port").toInt(53));
    const quint16 hInPort = static_cast<quint16>(config.value("h_in_port").toInt(0));

    // --- precompute send-domain table ---
    const qint64 maxEncoded = maxDomainLen + 2;
    for (const QByteArray &d : sendDoms) {
        SendDomain sd;
        sd.qnameEncoded = encodeQname(d.toLower());
        int chunkLen = 0;
        if (!getChunkLen(maxEncoded, sd.qnameEncoded.size(), maxSubLen, kDataOffsetWidth,
                         chunkLen)) {
            fail(QStringLiteral("send domain '%1' leaves no room for data")
                         .arg(QString::fromLatin1(d)));
            return false;
        }
        sd.chunkLen = chunkLen;
        sendDomains.append(sd);
    }
    for (const QByteArray &d : recvDoms) {
        recvDomains.append(labelDomain(d.toLower()));
    }

    // --- bind local app (h_in) socket on loopback: AmneziaWG targets this ---
    hIn = new QUdpSocket(this);
    if (!hIn->bind(QHostAddress(QStringLiteral("127.0.0.1")), hInPort)) {
        fail(QStringLiteral("failed to bind h_in 127.0.0.1:%1: %2")
                     .arg(hInPort)
                     .arg(hIn->errorString()));
        return false;
    }
    connect(hIn, &QUdpSocket::readyRead, this, &EnginePrivate::onHInReadyRead);

    // --- bind authoritative listener (wan) ---
    wan = new QUdpSocket(this);
    QHostAddress recvBind(recvIfaceIp);
    if (recvBind.isNull()) {
        recvBind = QHostAddress::AnyIPv4;
    }
    if (!wan->bind(recvBind, recvPort)) {
        fail(QStringLiteral("failed to bind receiver %1:%2: %3")
                     .arg(recvIfaceIp)
                     .arg(recvPort)
                     .arg(wan->errorString()));
        return false;
    }
    connect(wan, &QUdpSocket::readyRead, this, &EnginePrivate::onWanReadyRead);

    // --- bind send sockets (port spreading) ---
    QHostAddress sendBind(sendIfaceIp);
    if (sendBind.isNull()) {
        sendBind = QHostAddress::AnyIPv4;
    }
    for (int i = 0; i < sendSockCount; ++i) {
        auto *s = new QUdpSocket(this);
        if (!s->bind(sendBind, 0)) {
            fail(QStringLiteral("failed to bind send socket on %1: %2")
                         .arg(sendIfaceIp)
                         .arg(s->errorString()));
            return false;
        }
        sendSocks.append(s);
    }

    // --- resolvers + per-resolver pacing timers ---
    for (const QByteArray &ip : dnsIps) {
        auto r = std::make_unique<Resolver>();
        if (!parseResolver(QString::fromLatin1(ip), r->addr, r->port)) {
            fail(QStringLiteral("cannot parse dns_ip '%1'").arg(QString::fromLatin1(ip)));
            return false;
        }
        Resolver *rp = r.get();
        rp->timer = new QTimer(this);
        rp->timer->setInterval(static_cast<int>(sendIntervalMs));
        rp->timer->setTimerType(Qt::PreciseTimer);
        connect(rp->timer, &QTimer::timeout, this, [this, rp]() { onResolverTimer(rp); });
        resolvers.push_back(std::move(r));
    }

    reasm = std::make_unique<DataHandler>(static_cast<int>(kTotalDataOffset), kAssembleMs);
    sweepTimer = new QTimer(this);
    sweepTimer->setInterval(static_cast<int>(kAssembleMs / 2));
    connect(sweepTimer, &QTimer::timeout, this, [this]() {
        if (reasm) {
            reasm->sweep(nowMs());
        }
    });
    sweepTimer->start();

    // Seed rolling indices so distinct starts don't all bias the first packets
    // onto one socket/resolver/offset.
    auto *rng = QRandomGenerator::global();
    sendSockIndex = static_cast<int>(rng->bounded(static_cast<quint32>(sendSocks.size())));
    queryId = static_cast<quint16>(rng->bounded(0x10000u));
    dataOffset = rng->bounded(kTotalDataOffset);
    resIndex = static_cast<int>(rng->bounded(static_cast<quint32>(resolvers.size())));
    sendDomainIndex = static_cast<int>(rng->bounded(static_cast<quint32>(sendDomains.size())));

    qCInfo(qqdnsEngine) << "qqdns engine connected: localUdpPort=" << hIn->localPort()
                        << "listen=" << recvIfaceIp << ":" << recvPort
                        << "resolvers=" << resolvers.size() << "sendSocks=" << sendSocks.size();
    setState(Engine::State::Connected);
    return true;
}

void EnginePrivate::onHInReadyRead()
{
    const int nDomains = sendDomains.size();
    const int nRes = static_cast<int>(resolvers.size());
    const int nSocks = sendSocks.size();

    while (hIn && hIn->hasPendingDatagrams()) {
        QByteArray buf(static_cast<int>(hIn->pendingDatagramSize()), Qt::Uninitialized);
        QHostAddress src;
        quint16 srcPort = 0;
        const qint64 n = hIn->readDatagram(buf.data(), buf.size(), &src, &srcPort);
        if (n <= 0) {
            continue;
        }
        buf.truncate(static_cast<int>(n));

        // Client role: learn the app (AmneziaWG) address from whoever sends.
        if (!lastHValid || lastHAddr != src || lastHPort != srcPort) {
            lastHAddr = src;
            lastHPort = srcPort;
            lastHValid = true;
            qCInfo(qqdnsEngine) << "qqdns: local app address learned" << src << srcPort;
        }
        bytesSent += static_cast<quint64>(buf.size());
        emit bytesChanged(0, static_cast<quint64>(buf.size()));

        QList<QByteArray> finalDomains = getBase32FinalDomains(
                buf, dataOffset, sendDomainIndex, sendDomains, maxSubLen, kDataOffsetWidth,
                maxDomainLen + 2);
        if (finalDomains.isEmpty()) {
            qCWarning(qqdnsEngine) << "qqdns: datagram too large for max_domain_len, dropped"
                                   << buf.size();
            continue;
        }
        dataOffset = (dataOffset + 1) & (kTotalDataOffset - 1);
        sendDomainIndex = (sendDomainIndex + finalDomains.size()) % nDomains;

        // Build per-fragment DNS query datagrams (sockIndex assigned round-robin).
        struct Frag {
            int sockIndex;
            QByteArray query;
        };
        QVector<Frag> frags;
        frags.reserve(finalDomains.size());
        for (const QByteArray &fd : finalDomains) {
            frags.append({ sendSockIndex, buildDnsQuery(fd, queryId, sendQueryType) });
            sendSockIndex = (sendSockIndex + 1) % nSocks;
            queryId = static_cast<quint16>(queryId + 1);
        }

        // Enqueue `tries` copies, round-robining across resolver queues.
        const qint64 now = nowMs();
        for (int t = 0; t < tries; ++t) {
            Resolver *r = resolvers[resIndex].get();
            for (const Frag &f : frags) {
                r->queue.enqueue({ f.sockIndex, f.query, now });
            }
            if (!r->timer->isActive()) {
                r->timer->start();
            }
            resIndex = (resIndex + 1) % nRes;
        }
    }
}

void EnginePrivate::onResolverTimer(Resolver *r)
{
    // Discard datagrams that waited past the limit, then send one.
    const qint64 now = nowMs();
    while (!r->queue.isEmpty() && (now - r->queue.head().entryMs) > waitLimitMs) {
        r->queue.dequeue();
    }
    if (r->queue.isEmpty()) {
        r->timer->stop();
        return;
    }
    const PendingSend job = r->queue.dequeue();
    if (job.sockIndex >= 0 && job.sockIndex < sendSocks.size()) {
        sendSocks[job.sockIndex]->writeDatagram(job.query, r->addr, r->port);
    }
    if (r->queue.isEmpty()) {
        r->timer->stop();
    }
}

void EnginePrivate::onWanReadyRead()
{
    while (wan && wan->hasPendingDatagrams()) {
        QByteArray raw(static_cast<int>(wan->pendingDatagramSize()), Qt::Uninitialized);
        QHostAddress resolverSrc;
        quint16 resolverPort = 0;
        const qint64 n = wan->readDatagram(raw.data(), raw.size(), &resolverSrc, &resolverPort);
        if (n <= 0) {
            continue;
        }
        raw.truncate(static_cast<int>(n));

        const ParsedQuery parsed = handleDnsRequest(raw);
        if (!parsed.ok) {
            continue; // not a query we understand — no response
        }
        const int suffix = matchRecvSuffix(parsed.labels, recvDomains);
        if (suffix < 0) {
            continue; // not addressed to our delegated domain
        }

        // Reassemble + deliver (best-effort; we still answer the resolver below).
        QByteArray dataWithHeader;
        for (int i = 0; i < parsed.labels.size() - suffix; ++i) {
            dataWithHeader.append(parsed.labels[i]);
        }
        if (!dataWithHeader.isEmpty()) {
            const ChunkData cd = getChunkData(dataWithHeader, kDataOffsetWidth);
            const bool bad = cd.fragmentPart == 63 && !cd.lastFragment;
            if (cd.ok && !bad && !cd.chunk.isEmpty()) {
                const DataHandler::Result res = reasm->newDataEvent(
                        cd.dataOffset, cd.fragmentPart, cd.lastFragment, cd.chunk, nowMs());
                if (res.complete) {
                    QByteArray payload;
                    if (b32DecodeNoPad(res.joined, payload)) {
                        if (lastHValid && hIn) {
                            hIn->writeDatagram(payload, lastHAddr, lastHPort);
                            bytesRecv += static_cast<quint64>(payload.size());
                            emit bytesChanged(static_cast<quint64>(payload.size()), 0);
                        }
                    } else {
                        qCDebug(qqdnsEngine) << "qqdns: base32 decode failed";
                    }
                }
            }
        }

        // Always answer the resolver with NOERROR/empty so recursion stays healthy.
        const QByteArray question = raw.mid(12, parsed.nextQuestion - 12);
        const QByteArray response =
                createNoerrorEmptyResponse(parsed.qid, parsed.qflags, question);
        wan->writeDatagram(response, resolverSrc, resolverPort);
    }
}

void EnginePrivate::teardown()
{
    if (sweepTimer) {
        sweepTimer->stop();
        sweepTimer->deleteLater();
        sweepTimer = nullptr;
    }
    for (auto &r : resolvers) {
        if (r->timer) {
            r->timer->stop();
            r->timer->deleteLater();
            r->timer = nullptr;
        }
    }
    resolvers.clear();
    for (QUdpSocket *s : sendSocks) {
        s->close();
        s->deleteLater();
    }
    sendSocks.clear();
    if (wan) {
        wan->close();
        wan->deleteLater();
        wan = nullptr;
    }
    if (hIn) {
        hIn->close();
        hIn->deleteLater();
        hIn = nullptr;
    }
    reasm.reset();
    sendDomains.clear();
    recvDomains.clear();
    lastHValid = false;
}

// ---------------------------------------------------------------------------

Engine::Engine(QObject *parent) : QObject(parent), d(std::make_unique<EnginePrivate>(this))
{
    connect(d.get(), &EnginePrivate::stateChanged, this, &Engine::stateChanged);
    connect(d.get(), &EnginePrivate::bytesChanged, this, &Engine::bytesChanged);
}

Engine::~Engine()
{
    Engine::stop();
}

bool Engine::start(const QJsonObject &config)
{
    if (d->state == State::Connected || d->state == State::Starting) {
        return true;
    }
    d->lastError.clear();
    return d->startEngine(config);
}

void Engine::stop()
{
    if (d->state == State::Idle) {
        return;
    }
    d->setState(State::Stopping);
    d->teardown();
    d->setState(State::Idle);
}

Engine::State Engine::state() const
{
    return d->state;
}

QString Engine::lastError() const
{
    return d->lastError;
}

quint16 Engine::localUdpPort() const
{
    return d->hIn ? d->hIn->localPort() : 0;
}

quint64 Engine::bytesReceived() const
{
    return d->bytesRecv;
}

quint64 Engine::bytesSent() const
{
    return d->bytesSent;
}

} // namespace amnezia::qqdns

#include "engine.moc"
