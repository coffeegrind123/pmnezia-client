// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TRUSTTUNNELPROTOCOL_H
#define TRUSTTUNNELPROTOCOL_H

#include <QHostAddress>
#include <QList>
#include <QSharedPointer>
#include <QStringList>
#include <QTimer>

#include "core/utils/ipcClient.h"
#include "core/utils/routeModes.h"
#include "core/utils/serialization/trustTunnel.h"
#include "vpnProtocol.h"

// Desktop TrustTunnel: upstream's trusttunnel_client runs as a privileged
// process in SOCKS5 mode and tun2socks carries the TUN into it, the same
// TUN + routing path XrayProtocol uses.
//
//   apps -> TUN -> tun2socks -> socks5://127.0.0.1:<port> -> trusttunnel_client
//        -> HTTP/2 (TLS/TCP) or HTTP/3 (QUIC/UDP) -> endpoint
//
// In SOCKS mode trusttunnel_client does not pin its own sockets to the
// physical interface (its protect callback is TUN-only), so the endpoint IPs
// get host routes via the physical gateway and kill-switch allowances.
class TrustTunnelProtocol : public VpnProtocol
{
public:
    explicit TrustTunnelProtocol(const QJsonObject &configuration, QObject *parent = nullptr);
    ~TrustTunnelProtocol() override;

    ErrorCode start() override;
    void stop() override;

private:
    enum class Stage { Idle, StartingClient, StartingTun2Socks, Connected };

    ErrorCode resolveEndpoint();
    ErrorCode startClient(const QString &configPath);
    ErrorCode startTun2Socks();
    ErrorCode setupRouting();
    void onClientOutput(const QString &chunk);
    void fail(ErrorCode code);
    void stopProcess(QSharedPointer<IpcProcessInterfaceReplica> &process, bool graceful);

    amnezia::serialization::trusttunnel::Endpoint m_endpoint;
    amnezia::RouteMode m_routeMode;
    QList<QHostAddress> m_dnsServers;
    QString m_remoteAddress;
    // Endpoint addresses rewritten to resolved IPs, and those IPs as /32 or
    // /128 for the route and kill-switch exemptions.
    QStringList m_resolvedAddresses;
    QStringList m_endpointRoutes;
    bool m_endpointRoutesAdded = false;

    QString m_socksUser;
    QString m_socksPassword;
    quint16 m_socksPort = 0;

    Stage m_stage = Stage::Idle;
    QString m_clientOutputTail;
    QTimer m_connectTimer;
    QSharedPointer<IpcProcessInterfaceReplica> m_clientProcess;
    QSharedPointer<IpcProcessInterfaceReplica> m_tun2socksProcess;
    int m_tun2socksRetryCount = 0;

    static constexpr int connectTimeoutMs = 30000;
    static constexpr int maxTun2SocksRetries = 5;
    static constexpr int tun2socksRetryDelayMs = 400;
};

#endif // TRUSTTUNNELPROTOCOL_H
