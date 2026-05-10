#include "masterDnsVpnProtocol.h"

#include "core/protocols/protocolUtils.h"
#include "core/utils/constants/configKeys.h"
#include "core/utils/constants/protocolConstants.h"
#include "core/utils/ipcClient.h"
#include "core/utils/networkUtilities.h"
#include "ipc.h"

#include <QDeadlineTimer>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QTcpSocket>
#include <QTextStream>
#include <QtCore/qprocess.h>

namespace {
#ifdef Q_OS_MACOS
constexpr char kTunName[] = "utun24";
#else
constexpr char kTunName[] = "tun3";
#endif

// Upper bound on how long we wait for the bundled mdnsvpn client to come
// up and bind its local SOCKS5 listener. mdnsvpn does MTU discovery
// against every public resolver before serving traffic, which can take
// several seconds on a fresh start. 60 s is generous on broadband and
// borderline on flaky tethers.
constexpr int kSocksListenerWaitMs = 60'000;
constexpr int kSocksListenerPollMs = 250;
} // namespace

MasterDnsVpnProtocol::MasterDnsVpnProtocol(const QJsonObject &configuration, QObject *parent)
    : VpnProtocol(configuration, parent)
{
    m_vpnGateway = amnezia::protocols::masterDnsVpn::defaultLocalAddr;
    m_vpnLocalAddress = amnezia::protocols::masterDnsVpn::defaultLocalAddr;
    m_routeGateway = NetworkUtilities::getGatewayAndIface().first;

    m_routeMode = static_cast<amnezia::RouteMode>(
            configuration.value(amnezia::configKey::splitTunnelType).toInt());
    m_remoteAddress = NetworkUtilities::getIPAddress(
            m_rawConfig.value(amnezia::configKey::hostName).toString());

    const QString primaryDns = configuration.value(amnezia::configKey::dns1).toString();
    if (!primaryDns.isEmpty()) {
        m_dnsServers.push_back(QHostAddress(primaryDns));
    }
    if (primaryDns != amnezia::protocols::dns::amneziaDnsIp) {
        const QString secondaryDns = configuration.value(amnezia::configKey::dns2).toString();
        if (!secondaryDns.isEmpty()) {
            m_dnsServers.push_back(QHostAddress(secondaryDns));
        }
    }

    m_mdnsvpnConfig = configuration.value(ProtocolUtils::key_proto_config_data(Proto::MasterDnsVpn))
                              .toObject();
    if (m_mdnsvpnConfig.isEmpty()) {
        qWarning() << "MasterDnsVpn config wrapper is empty";
    }

    // The operator's verbatim client_config.toml lives at .config — the
    // model's setNativeConfig() puts it there. We carry it through the
    // wire JSON unmodified so that whatever upstream knobs the operator
    // chose (RESOLVERS, RESOLVER_BALANCING_STRATEGY, COMPRESSION, …) end
    // up on disk exactly as they hand-crafted them.
    m_clientConfigToml = m_mdnsvpnConfig.value(amnezia::configKey::config).toString();

    const QString listenPortStr = m_mdnsvpnConfig.value(amnezia::configKey::mdvListenPort).toString();
    bool listenPortOk = false;
    const int listenPort = listenPortStr.toInt(&listenPortOk);
    if (listenPortOk && listenPort > 0 && listenPort <= 65'535) {
        m_socksPort = listenPort;
    }
}

MasterDnsVpnProtocol::~MasterDnsVpnProtocol()
{
    qDebug() << "MasterDnsVpnProtocol::~MasterDnsVpnProtocol()";
    MasterDnsVpnProtocol::stop();
}

ErrorCode MasterDnsVpnProtocol::start()
{
    qDebug() << "MasterDnsVpnProtocol::start()";

    if (m_clientConfigToml.trimmed().isEmpty()) {
        qCritical() << "MasterDnsVpn client_config.toml is empty";
        return ErrorCode::InternalError;
    }

    if (auto rc = writeRuntimeFiles(); rc != ErrorCode::NoError) {
        return rc;
    }

    if (auto rc = startMdnsvpn(); rc != ErrorCode::NoError) {
        return rc;
    }

    if (auto rc = waitForSocksListener(); rc != ErrorCode::NoError) {
        stop();
        return rc;
    }

    return startTun2Socks();
}

void MasterDnsVpnProtocol::stop()
{
    qDebug() << "MasterDnsVpnProtocol::stop()";

    IpcClient::withInterface([](QSharedPointer<IpcInterfaceReplica> iface) {
        auto disableKillSwitch = iface->disableKillSwitch();
        if (!disableKillSwitch.waitForFinished() || !disableKillSwitch.returnValue())
            qWarning() << "Failed to disable killswitch";

        auto StartRoutingIpv6 = iface->StartRoutingIpv6();
        if (!StartRoutingIpv6.waitForFinished() || !StartRoutingIpv6.returnValue())
            qWarning() << "Failed to start routing ipv6";

        auto restoreResolvers = iface->restoreResolvers();
        if (!restoreResolvers.waitForFinished() || !restoreResolvers.returnValue())
            qWarning() << "Failed to restore resolvers";

        auto deleteTun = iface->deleteTun(kTunName);
        if (!deleteTun.waitForFinished() || !deleteTun.returnValue())
            qWarning() << "Failed to delete tun";
    });

    auto terminate = [](QSharedPointer<IpcProcessInterfaceReplica> &proc, const char *label) {
        if (!proc) {
            return;
        }
        proc->blockSignals(true);
#ifndef Q_OS_WIN
        proc->terminate();
        auto waitForFinished = proc->waitForFinished(1'000);
        if (!waitForFinished.waitForFinished() || !waitForFinished.returnValue()) {
            qWarning() << "Failed to terminate" << label << "; killing the process";
            proc->kill();
        }
#else
        // terminate() is a no-op on Windows console children.
        proc->kill();
#endif
        proc->close();
        proc.reset();
    };

    terminate(m_tun2socksProcess, "tun2socks");
    terminate(m_mdnsvpnProcess, "mdnsvpn");

    m_workDir.reset(); // RAII-removes the temp dir + its keyfile.

    setConnectionState(Vpn::ConnectionState::Disconnected);
}

ErrorCode MasterDnsVpnProtocol::writeRuntimeFiles()
{
    m_workDir.reset(new QTemporaryDir());
    if (!m_workDir->isValid()) {
        qCritical() << "Failed to create mdnsvpn work dir:" << m_workDir->errorString();
        return ErrorCode::InternalError;
    }
    // QTemporaryDir defaults to autoRemove=true — RAII deletes the temp
    // dir (and the operator's encryption key) when m_workDir resets.

    m_configPath = m_workDir->filePath(QStringLiteral("client_config.toml"));
    QFile cfg(m_configPath);
    if (!cfg.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCritical() << "Failed to open" << m_configPath << "for writing:" << cfg.errorString();
        return ErrorCode::InternalError;
    }
    cfg.write(m_clientConfigToml.toUtf8());
    cfg.close();
#ifndef Q_OS_WIN
    cfg.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
#endif

    return ErrorCode::NoError;
}

ErrorCode MasterDnsVpnProtocol::startMdnsvpn()
{
    m_mdnsvpnProcess = IpcClient::CreatePrivilegedProcess();
    if (!m_mdnsvpnProcess->waitForSource()) {
        qCritical() << "Failed to acquire privileged process slot for mdnsvpn";
        return ErrorCode::AmneziaServiceConnectionFailed;
    }

    m_mdnsvpnProcess->setProgram(amnezia::PermittedProcess::MasterDnsVpn);
    m_mdnsvpnProcess->setArguments({ QStringLiteral("-config"), m_configPath,
                                     QStringLiteral("-nowait") });

    connect(m_mdnsvpnProcess.data(), &IpcProcessInterfaceReplica::readyReadStandardError, this,
            [this]() {
                auto reply = m_mdnsvpnProcess->readAllStandardError();
                if (!reply.waitForFinished()) {
                    return;
                }
                const QString line = reply.returnValue().trimmed();
                if (!line.isEmpty()) {
                    qDebug() << "[mdnsvpn]:" << line;
                }
            },
            Qt::QueuedConnection);

    connect(m_mdnsvpnProcess.data(), &IpcProcessInterfaceReplica::finished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                if (exitStatus == QProcess::ExitStatus::CrashExit) {
                    qCritical() << "mdnsvpn process crashed";
                } else {
                    qCritical() << QString("mdnsvpn process exited with code %1").arg(exitCode);
                }
                stop();
                setLastError(ErrorCode::InternalError);
            },
            Qt::QueuedConnection);

    m_mdnsvpnProcess->start();
    return ErrorCode::NoError;
}

ErrorCode MasterDnsVpnProtocol::waitForSocksListener()
{
    QDeadlineTimer deadline(kSocksListenerWaitMs);
    while (!deadline.hasExpired()) {
        QTcpSocket probe;
        probe.connectToHost(QHostAddress::LocalHost, m_socksPort);
        if (probe.waitForConnected(kSocksListenerPollMs)) {
            qDebug() << "mdnsvpn SOCKS5 listener up on 127.0.0.1:" << m_socksPort;
            return ErrorCode::NoError;
        }
        // waitForConnected itself burns up to kSocksListenerPollMs ms;
        // no extra sleep needed before the next iteration.
    }
    qCritical() << "mdnsvpn SOCKS5 listener did not come up within"
                << kSocksListenerWaitMs << "ms";
    return ErrorCode::InternalError;
}

ErrorCode MasterDnsVpnProtocol::startTun2Socks()
{
    m_tun2socksProcess = IpcClient::CreatePrivilegedProcess();
    if (!m_tun2socksProcess->waitForSource()) {
        return ErrorCode::AmneziaServiceConnectionFailed;
    }

    const QString proxyUrl = QStringLiteral("socks5://127.0.0.1:%1").arg(m_socksPort);

    m_tun2socksProcess->setProgram(amnezia::PermittedProcess::Tun2Socks);
    m_tun2socksProcess->setArguments({ QStringLiteral("-device"),
                                       QStringLiteral("tun://%1").arg(kTunName),
                                       QStringLiteral("-proxy"), proxyUrl });

    connect(m_tun2socksProcess.data(), &IpcProcessInterfaceReplica::readyReadStandardError, this,
            [this]() {
                auto readAllStandardError = m_tun2socksProcess->readAllStandardError();
                if (!readAllStandardError.waitForFinished()) {
                    qWarning() << "Failed to read output from tun2socks";
                    return;
                }
                const QString line = readAllStandardError.returnValue();

                if (!line.contains("[TCP]") && !line.contains("[UDP]"))
                    qDebug() << "[tun2socks]:" << line;

                if (line.contains("[STACK] tun://") && line.contains("<-> socks5://")) {
                    disconnect(m_tun2socksProcess.data(),
                               &IpcProcessInterfaceReplica::readyReadStandardOutput, this, nullptr);

                    if (ErrorCode res = setupRouting(); res != ErrorCode::NoError) {
                        stop();
                        setLastError(res);
                    } else {
                        setConnectionState(Vpn::ConnectionState::Connected);
                    }
                }
            },
            Qt::QueuedConnection);

    connect(m_tun2socksProcess.data(), &IpcProcessInterfaceReplica::finished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                if (exitStatus == QProcess::ExitStatus::CrashExit) {
                    qCritical() << "Tun2socks process crashed (mdnsvpn flow)";
                } else {
                    qCritical() << QString("Tun2socks process exited with code %1 (mdnsvpn flow)")
                                           .arg(exitCode);
                }
                stop();
                setLastError(ErrorCode::Tun2SockExecutableCrashed);
            },
            Qt::QueuedConnection);

    m_tun2socksProcess->start();
    return ErrorCode::NoError;
}

ErrorCode MasterDnsVpnProtocol::setupRouting()
{
    return IpcClient::withInterface(
            [this](QSharedPointer<IpcInterfaceReplica> iface) -> ErrorCode {
#ifdef Q_OS_WIN
                const int inetAdapterIndex = NetworkUtilities::AdapterIndexTo(QHostAddress(m_remoteAddress));
#endif
                auto createTun = iface->createTun(kTunName,
                                                  amnezia::protocols::masterDnsVpn::defaultLocalAddr);
                if (!createTun.waitForFinished() || !createTun.returnValue()) {
                    qCritical() << "Failed to assign IP address for TUN";
                    return ErrorCode::InternalError;
                }

                auto updateResolvers = iface->updateResolvers(kTunName, m_dnsServers);
                if (!updateResolvers.waitForFinished() || !updateResolvers.returnValue()) {
                    qCritical() << "Failed to set DNS resolvers for TUN";
                    return ErrorCode::InternalError;
                }

#ifdef Q_OS_WIN
                int vpnAdapterIndex = -1;
                QList<QNetworkInterface> netInterfaces = QNetworkInterface::allInterfaces();
                for (auto &netInterface : netInterfaces) {
                    for (auto &address : netInterface.addressEntries()) {
                        if (m_vpnLocalAddress == address.ip().toString())
                            vpnAdapterIndex = netInterface.index();
                    }
                }
#else
                static const int vpnAdapterIndex = 0;
#endif
                const bool killSwitchEnabled =
                        QVariant(m_rawConfig.value(amnezia::configKey::killSwitchOption).toString()).toBool();
                if (killSwitchEnabled) {
                    if (vpnAdapterIndex != -1) {
                        QJsonObject config = m_rawConfig;
                        config.insert("vpnServer", m_remoteAddress);

                        auto enableKillSwitch =
                                IpcClient::Interface()->enableKillSwitch(config, vpnAdapterIndex);
                        if (!enableKillSwitch.waitForFinished() || !enableKillSwitch.returnValue()) {
                            qCritical() << "Failed to enable killswitch";
                            return ErrorCode::InternalError;
                        }
                    } else {
                        qWarning() << "Failed to get vpnAdapterIndex. Killswitch disabled";
                    }
                }

                if (m_routeMode == amnezia::RouteMode::VpnAllSites) {
                    static const QStringList subnets = { "1.0.0.0/8",  "2.0.0.0/7",  "4.0.0.0/6",
                                                         "8.0.0.0/5",  "16.0.0.0/4", "32.0.0.0/3",
                                                         "64.0.0.0/2", "128.0.0.0/1" };

                    auto routeAddList = iface->routeAddList(m_vpnGateway, subnets);
                    if (!routeAddList.waitForFinished()
                        || routeAddList.returnValue() != subnets.count()) {
                        qCritical() << "Failed to set routes for TUN";
                        return ErrorCode::InternalError;
                    }
                }

                auto StopRoutingIpv6 = iface->StopRoutingIpv6();
                if (!StopRoutingIpv6.waitForFinished() || !StopRoutingIpv6.returnValue()) {
                    qCritical() << "Failed to disable IPv6 routing";
                    return ErrorCode::InternalError;
                }

#ifdef Q_OS_WIN
                if (inetAdapterIndex != -1 && vpnAdapterIndex != -1) {
                    QJsonObject config = m_rawConfig;
                    config.insert("inetAdapterIndex", inetAdapterIndex);
                    config.insert("vpnAdapterIndex", vpnAdapterIndex);
                    config.insert("vpnGateway", m_vpnGateway);
                    config.insert("vpnServer", m_remoteAddress);

                    auto enablePeerTraffic = iface->enablePeerTraffic(config);
                    if (!enablePeerTraffic.waitForFinished() || !enablePeerTraffic.returnValue()) {
                        qCritical() << "Failed to enable peer traffic";
                        return ErrorCode::InternalError;
                    }
                } else {
                    qWarning() << "Failed to get adapter indexes. Split-tunneling disabled";
                }
#endif
                return ErrorCode::NoError;
            },
            []() { return ErrorCode::AmneziaServiceConnectionFailed; });
}
