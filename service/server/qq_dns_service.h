// SPDX-License-Identifier: GPL-3.0-or-later
//
// Service-side singleton that hosts the native QQ-DNS engine in the privileged
// daemon process. Mirrors service/server/master_dns_vpn_service.{h,cpp}; the
// GUI client speaks to us via the IPC slots `qqDnsStart` / `qqDnsStop` /
// `qqDnsLocalPort`.
//
// The engine lives in the privileged daemon because it must bind the
// authoritative :53 listener and — the runtime TODO — its outbound resolver
// sockets need to be pinned to the physical interface so they don't loop back
// through the AmneziaWG TUN that runs on top of the tunnel.

#ifndef QQ_DNS_SERVICE_H
#define QQ_DNS_SERVICE_H

#include <QObject>
#include <QString>
#include <QtGlobal>

#include <memory>

namespace amnezia::qqdns {
class Engine;
}

class QqDnsService : public QObject
{
    Q_OBJECT

public:
    static QqDnsService &getInstance();

    // configJson is the QQ-DNS wrapper the model emits (see
    // client/core/models/protocols/qqDnsProtocolConfig.h) — the engine reads
    // the snake_case fields directly and ignores the embedded "awg" object.
    bool start(const QString &configJson);
    bool stop();

    // The loopback UDP port AmneziaWG points its endpoint at. 0 when idle.
    quint16 localUdpPort() const;

private:
    QqDnsService();
    ~QqDnsService();
    Q_DISABLE_COPY_MOVE(QqDnsService)

    std::unique_ptr<amnezia::qqdns::Engine> m_engine;
};

#endif // QQ_DNS_SERVICE_H
