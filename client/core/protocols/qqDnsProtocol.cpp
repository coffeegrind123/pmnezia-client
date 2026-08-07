// SPDX-License-Identifier: GPL-3.0-or-later

#include "qqDnsProtocol.h"

#include "core/protocols/protocolUtils.h"
#include "core/protocols/wireGuardProtocol.h"
#include "core/utils/constants/configKeys.h"
#include "core/utils/ipcClient.h"
#include "core/utils/networkUtilities.h"
#include "ipc.h"

#include <QJsonArray>
#include <QJsonDocument>

QqDnsProtocol::QqDnsProtocol(const QJsonObject &configuration, QObject *parent)
    : VpnProtocol(configuration, parent)
{
    // The wrapper the model stores is already the engine's snake_case blob plus
    // an embedded "awg" object; the engine ignores keys it doesn't recognise,
    // so we pass the whole thing through as its config.
    m_engineConfig =
            configuration.value(ProtocolUtils::key_proto_config_data(Proto::QqDns)).toObject();

    // Physical default gateway, captured before the tunnel comes up — the
    // resolver route exemptions are pinned via it.
    m_routeGateway = NetworkUtilities::getGatewayAndIface().first;

    // Resolver IPs (strip any "ip:port" / "[v6]:port") as /32 (or /128) routes.
    const QJsonArray dnsIps = m_engineConfig.value(configKey::qqDnsIps).toArray();
    for (const QJsonValue &v : dnsIps) {
        QString ip = v.toString().trimmed();
        if (ip.isEmpty()) {
            continue;
        }
        bool v6 = false;
        if (ip.startsWith('[')) {
            const int c = ip.indexOf(']');
            if (c > 0) {
                ip = ip.mid(1, c - 1);
            }
            v6 = true;
        } else if (ip.indexOf(':') > 0 && ip.indexOf(':') == ip.lastIndexOf(':')) {
            ip = ip.left(ip.indexOf(':'));
        } else if (ip.contains(':')) {
            v6 = true; // bare IPv6
        }
        m_resolverRoutes.append(ip + (v6 ? QStringLiteral("/128") : QStringLiteral("/32")));
    }
}

QqDnsProtocol::~QqDnsProtocol()
{
    qDebug() << "QqDnsProtocol::~QqDnsProtocol()";
    QqDnsProtocol::stop();
}

QJsonObject QqDnsProtocol::buildInnerAwgConfig(quint16 enginePort) const
{
    const QJsonObject wrapper = m_engineConfig;
    const QJsonObject awgWrapper = wrapper.value(configKey::qqAwg).toObject();

    QJsonObject awgConfigData =
            awgWrapper.value(ProtocolUtils::key_proto_config_data(Proto::Awg)).toObject();
    // Rewrite the WireGuard endpoint to the loopback engine port. Loopback is
    // never routed through the TUN, so the handshake reaches the engine.
    const QString loopback = QStringLiteral("127.0.0.1");
    awgConfigData[configKey::hostName] = loopback;
    awgConfigData[configKey::port] = enginePort;

    // Inherit the user's runtime preferences (split tunnel, killswitch, DNS,
    // client id, …) from the outer config; swap the protocol + data to awg.
    QJsonObject awgRaw = m_rawConfig;
    awgRaw.remove(ProtocolUtils::key_proto_config_data(Proto::QqDns));
    awgRaw[QStringLiteral("protocol")] = ProtocolUtils::protoToString(Proto::Awg);
    awgRaw[ProtocolUtils::key_proto_config_data(Proto::Awg)] = awgConfigData;
    awgRaw[configKey::hostName] = loopback;
    awgRaw[configKey::port] = enginePort;
    return awgRaw;
}

ErrorCode QqDnsProtocol::start()
{
    qDebug() << "QqDnsProtocol::start()";

    if (m_engineConfig.isEmpty()) {
        qCritical() << "QqDns config wrapper is empty";
        return ErrorCode::InternalError;
    }

    const QString configJson =
            QString::fromUtf8(QJsonDocument(m_engineConfig).toJson(QJsonDocument::Compact));

    return IpcClient::withInterface(
            [&](QSharedPointer<IpcInterfaceReplica> iface) -> ErrorCode {
                auto startReply = iface->qqDnsStart(configJson);
                if (!startReply.waitForFinished() || !startReply.returnValue()) {
                    qCritical() << "Failed to start QqDns engine in service";
                    return ErrorCode::InternalError;
                }

                auto portReply = iface->qqDnsLocalPort();
                if (!portReply.waitForFinished()) {
                    qCritical() << "Failed to fetch QqDns local UDP port";
                    return ErrorCode::InternalError;
                }
                const quint16 enginePort = portReply.returnValue();
                if (enginePort == 0) {
                    qCritical() << "QqDns engine did not bind a local UDP port";
                    return ErrorCode::InternalError;
                }

                // Bring AmneziaWG up on top of the loopback engine port. The
                // factory constructs AWG containers as WireguardProtocol (it
                // reads the "protocol" field + awg_config_data), so do the same.
                const QJsonObject awgRaw = buildInnerAwgConfig(enginePort);
                m_awg = new WireguardProtocol(awgRaw, this);
                connect(m_awg.data(), &VpnProtocol::connectionStateChanged, this,
                        [this](Vpn::ConnectionState s) {
                            // Once Awg has installed its 0.0.0.0/0 route, pin the
                            // more-specific resolver exemptions on top so the
                            // engine's DNS queries don't loop into the tunnel.
                            if (s == Vpn::ConnectionState::Connected) {
                                addResolverRouteExemptions();
                            }
                            setConnectionState(s);
                        });
                connect(m_awg.data(), &VpnProtocol::protocolError, this,
                        [this](ErrorCode e) { setLastError(e); });

                const ErrorCode ec = m_awg->start();
                if (ec != ErrorCode::NoError) {
                    qCritical() << "Inner AmneziaWG failed to start (qqdns flow):" << ec;
                }
                return ec;
            },
            []() { return ErrorCode::AmneziaServiceConnectionFailed; });
}

void QqDnsProtocol::addResolverRouteExemptions()
{
    if (m_resolverRoutesAdded || m_resolverRoutes.isEmpty() || m_routeGateway.isEmpty()) {
        return;
    }
    m_resolverRoutesAdded = true;
    // NOTE: this is the portable "specific route wins" mechanism. On setups
    // where AmneziaWG uses fwmark + policy routing (rather than a plain
    // default route) the resolver traffic may instead need SO_MARK on the
    // engine sockets — that path needs on-device verification.
    IpcClient::withInterface([this](QSharedPointer<IpcInterfaceReplica> iface) {
        auto reply = iface->routeAddList(m_routeGateway, m_resolverRoutes);
        if (!reply.waitForFinished() || reply.returnValue() != m_resolverRoutes.size()) {
            qWarning() << "qqdns: failed to pin all resolver route exemptions via"
                       << m_routeGateway;
        } else {
            qDebug() << "qqdns: pinned" << m_resolverRoutes.size()
                     << "resolver route exemptions via" << m_routeGateway;
        }
    });
}

void QqDnsProtocol::stop()
{
    qDebug() << "QqDnsProtocol::stop()";

    if (m_awg) {
        m_awg->blockSignals(true);
        m_awg->stop();
        m_awg->deleteLater();
        m_awg.clear();
    }

    IpcClient::withInterface([](QSharedPointer<IpcInterfaceReplica> iface) {
        auto stopReply = iface->qqDnsStop();
        if (!stopReply.waitForFinished() || !stopReply.returnValue()) {
            qWarning() << "Failed to stop QqDns engine in service";
        }
    });

    setConnectionState(Vpn::ConnectionState::Disconnected);
}
