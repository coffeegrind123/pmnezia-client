#ifndef MASTERDNSVPNPROTOCOL_H
#define MASTERDNSVPNPROTOCOL_H

#include <QHostAddress>
#include <QList>
#include <QSharedPointer>
#include <QString>
#include <QTemporaryDir>

#include "core/utils/commonStructs.h"
#include "core/utils/errorCodes.h"
#include "core/utils/ipcClient.h"
#include "core/utils/routeModes.h"
#include "vpnProtocol.h"

class MasterDnsVpnProtocol : public VpnProtocol
{
    Q_OBJECT

public:
    MasterDnsVpnProtocol(const QJsonObject &configuration, QObject *parent = nullptr);
    ~MasterDnsVpnProtocol() override;

    ErrorCode start() override;
    void stop() override;

private:
    // Materialise the operator-supplied client_config.toml + a sibling
    // client_resolvers.txt into a private temp dir owned by m_workDir.
    // mdnsvpn reads the TOML's `RESOLVERS = […]` array, but we still write
    // the .txt for parity with the upstream layout — keeps the on-disk
    // surface predictable for support diagnostics.
    ErrorCode writeRuntimeFiles();

    // Spawn the mdnsvpn binary as a privileged child of the IPC service.
    ErrorCode startMdnsvpn();

    // Wait until the mdnsvpn client has bound its local SOCKS5 listener;
    // we poll connect(2) so tun2socks doesn't race the listener startup.
    ErrorCode waitForSocksListener();

    // Spawn tun2socks against the now-live SOCKS5 listener and bring up
    // the routing on success. Mirrors the corresponding xray flow.
    ErrorCode startTun2Socks();

    ErrorCode setupRouting();

    QJsonObject m_mdnsvpnConfig;
    amnezia::RouteMode m_routeMode = amnezia::RouteMode::VpnAllSites;
    QList<QHostAddress> m_dnsServers;
    QString m_remoteAddress;

    // The full TOML body the operator handed out for this peer (verbatim
    // round-trip from MasterDnsVpnClientConfig::nativeConfig).
    QString m_clientConfigToml;

    // Local SOCKS5 listen port the bundled mdnsvpn opens. Defaults to
    // 18000 (upstream sample); per-client override via the ConfigModel.
    int m_socksPort = 18000;

    QSharedPointer<QTemporaryDir> m_workDir;
    QString m_configPath;

    QSharedPointer<IpcProcessInterfaceReplica> m_mdnsvpnProcess;
    QSharedPointer<IpcProcessInterfaceReplica> m_tun2socksProcess;
};

#endif // MASTERDNSVPNPROTOCOL_H
