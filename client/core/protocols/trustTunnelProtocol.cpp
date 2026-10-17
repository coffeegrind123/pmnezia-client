// SPDX-License-Identifier: GPL-3.0-or-later

#include "trustTunnelProtocol.h"

#include "core/protocols/protocolUtils.h"
#include "core/utils/constants/configKeys.h"
#include "core/utils/constants/protocolConstants.h"
#include "core/utils/networkUtilities.h"
#include "core/utils/utilities.h"
#include "ipc.h"

#include <QJsonDocument>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QtCore/qprocess.h>

namespace tt = amnezia::serialization::trusttunnel;

namespace
{
#ifdef Q_OS_MACOS
constexpr char kTunName[] = "utun25";
#else
constexpr char kTunName[] = "tun4";
#endif

constexpr int socksCredentialLength = 16;

// Split "host:port" / "[v6]:port" / "host" into host and the ":port" suffix.
QPair<QString, QString> splitHostPort(const QString &address)
{
    if (address.startsWith('[')) {
        const int close = address.indexOf(']');
        if (close > 0) {
            return { address.mid(1, close - 1), address.mid(close + 1) };
        }
        return { address, QString() };
    }
    if (address.count(':') == 1) {
        const int colon = address.indexOf(':');
        return { address.left(colon), address.mid(colon) };
    }
    return { address, QString() };
}

quint16 acquireFreeLoopbackPort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0)) {
        return 0;
    }
    return probe.serverPort();
}

// Lines trusttunnel_client prints per proxied connection name destinations;
// keep them out of the app log.
bool isConnectionInfoLine(const QString &line)
{
    return line.contains(QStringLiteral(". Action: "));
}
} // namespace

TrustTunnelProtocol::TrustTunnelProtocol(const QJsonObject &configuration, QObject *parent)
    : VpnProtocol(configuration, parent)
{
    m_vpnGateway = amnezia::protocols::trustTunnel::defaultLocalAddr;
    m_vpnLocalAddress = amnezia::protocols::trustTunnel::defaultLocalAddr;
    m_routeGateway = NetworkUtilities::getGatewayAndIface().first;
    m_routeMode = static_cast<amnezia::RouteMode>(configuration.value(amnezia::configKey::splitTunnelType).toInt());

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

    m_endpoint = tt::Endpoint::fromJson(
            configuration.value(ProtocolUtils::key_proto_config_data(Proto::TrustTunnel)).toObject());

    m_connectTimer.setSingleShot(true);
    connect(&m_connectTimer, &QTimer::timeout, this, [this]() {
        qCritical() << "TrustTunnel: no connection to the endpoint within" << connectTimeoutMs << "ms";
        fail(ErrorCode::TrustTunnelConnectionFailed);
    });
}

TrustTunnelProtocol::~TrustTunnelProtocol()
{
    qDebug() << "TrustTunnelProtocol::~TrustTunnelProtocol()";
    TrustTunnelProtocol::stop();
}

ErrorCode TrustTunnelProtocol::resolveEndpoint()
{
    m_resolvedAddresses.clear();
    m_endpointRoutes.clear();

    // Resolve before the TUN exists: once the default routes point into the
    // tunnel, a lookup of the endpoint itself would have nowhere to go.
    for (const QString &address : m_endpoint.addresses) {
        const auto [host, portSuffix] = splitHostPort(address);
        const QString ip = NetworkUtilities::getIPAddress(host);
        const QHostAddress parsed(ip);
        if (parsed.isNull()) {
            qWarning() << "TrustTunnel: could not resolve endpoint address" << host;
            continue;
        }
        const bool v6 = parsed.protocol() == QAbstractSocket::IPv6Protocol;
        m_resolvedAddresses.append((v6 ? QStringLiteral("[%1]").arg(ip) : ip) + portSuffix);
        const QString route = ip + (v6 ? QStringLiteral("/128") : QStringLiteral("/32"));
        if (!m_endpointRoutes.contains(route)) {
            m_endpointRoutes.append(route);
        }
    }

    if (m_resolvedAddresses.isEmpty()) {
        return ErrorCode::TrustTunnelConnectionFailed;
    }
    m_remoteAddress = m_endpointRoutes.first().section('/', 0, 0);
    return ErrorCode::NoError;
}

ErrorCode TrustTunnelProtocol::start()
{
    qDebug() << "TrustTunnelProtocol::start()";

    if (const QString missing = m_endpoint.missingRequiredField(); !missing.isEmpty()) {
        qCritical() << "TrustTunnel config is missing" << missing;
        return ErrorCode::ImportInvalidConfigError;
    }
    if (m_endpoint.hostname.isEmpty() || m_endpoint.addresses.isEmpty()) {
        // A v2 subscription-only link: trusttunnel_client 1.1.x cannot fetch it.
        qCritical() << "TrustTunnel: subscription-only profiles are not supported by the bundled client";
        return ErrorCode::NotSupportedOnThisPlatform;
    }
    if (!m_endpoint.clientRandomAuthKey.isEmpty()) {
        qWarning() << "TrustTunnel: the bundled client ignores client_random_auth_key; an endpoint that requires it will refuse this connection";
    }

    if (ErrorCode e = resolveEndpoint(); e != ErrorCode::NoError) {
        return e;
    }

    m_socksPort = acquireFreeLoopbackPort();
    if (m_socksPort == 0) {
        qCritical() << "TrustTunnel: cannot reserve a loopback port for the SOCKS5 listener";
        return ErrorCode::InternalError;
    }
    m_socksUser = Utils::getRandomString(socksCredentialLength);
    m_socksPassword = Utils::getRandomString(socksCredentialLength);

    tt::Endpoint endpoint = m_endpoint;
    endpoint.addresses = m_resolvedAddresses;
    const QString endpointJson = QString::fromUtf8(QJsonDocument(endpoint.toJson()).toJson(QJsonDocument::Compact));
    const QString socksAddress = QStringLiteral("127.0.0.1:%1").arg(m_socksPort);

    return IpcClient::withInterface(
            [&](QSharedPointer<IpcInterfaceReplica> iface) -> ErrorCode {
                auto reply = iface->trustTunnelWriteConfig(endpointJson, socksAddress, m_socksUser, m_socksPassword);
                if (!reply.waitForFinished() || reply.returnValue().isEmpty()) {
                    qCritical() << "TrustTunnel: the service rejected the endpoint profile";
                    return ErrorCode::ImportInvalidConfigError;
                }
                const QString configPath = reply.returnValue();

                // Pin the endpoint to the physical gateway before anything is
                // routed into the TUN.
                if (!m_routeGateway.isEmpty()) {
                    auto routes = iface->routeAddList(m_routeGateway, m_endpointRoutes);
                    if (!routes.waitForFinished() || routes.returnValue() != m_endpointRoutes.size()) {
                        qWarning() << "TrustTunnel: could not pin every endpoint route via" << m_routeGateway;
                    } else {
                        m_endpointRoutesAdded = true;
                    }
                } else {
                    qWarning() << "TrustTunnel: no physical gateway found; the endpoint may loop into the tunnel";
                }

                return startClient(configPath);
            },
            []() { return ErrorCode::AmneziaServiceConnectionFailed; });
}

ErrorCode TrustTunnelProtocol::startClient(const QString &configPath)
{
    m_clientProcess = IpcClient::CreatePrivilegedProcess();
    if (!m_clientProcess || !m_clientProcess->waitForSource()) {
        return ErrorCode::AmneziaServiceConnectionFailed;
    }

    m_stage = Stage::StartingClient;
    m_clientOutputTail.clear();

    m_clientProcess->setProgram(amnezia::PermittedProcess::TrustTunnelClient);
    m_clientProcess->setArguments({ QStringLiteral("-c"), configPath });
    m_clientProcess->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_clientProcess.data(), &IpcProcessInterfaceReplica::readyReadStandardOutput, this,
            [this]() {
                if (!m_clientProcess) {
                    return;
                }
                auto out = m_clientProcess->readAllStandardOutput();
                if (out.waitForFinished()) {
                    onClientOutput(QString::fromUtf8(out.returnValue()));
                }
            },
            Qt::QueuedConnection);

    connect(m_clientProcess.data(), &IpcProcessInterfaceReplica::errorOccurred, this,
            [this](QProcess::ProcessError error) {
                if (error == QProcess::FailedToStart) {
                    qCritical() << "TrustTunnel: trusttunnel_client failed to start";
                    fail(ErrorCode::TrustTunnelExecutableMissing);
                }
            },
            Qt::QueuedConnection);

    connect(m_clientProcess.data(), &IpcProcessInterfaceReplica::finished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                qCritical() << "TrustTunnel: trusttunnel_client exited, code" << exitCode
                            << (exitStatus == QProcess::CrashExit ? "(crash)" : "");
                // It exits on its own when the endpoint refuses the
                // connection or the session drops for good.
                fail(m_stage == Stage::StartingClient ? ErrorCode::TrustTunnelConnectionFailed
                                                      : ErrorCode::TrustTunnelExecutableCrashed);
            },
            Qt::QueuedConnection);

    m_connectTimer.start(connectTimeoutMs);
    m_clientProcess->start();
    return ErrorCode::NoError;
}

void TrustTunnelProtocol::onClientOutput(const QString &chunk)
{
    m_clientOutputTail += chunk;
    const int lastNewline = m_clientOutputTail.lastIndexOf('\n');
    if (lastNewline < 0) {
        return;
    }
    const QStringList lines = m_clientOutputTail.left(lastNewline).split('\n', Qt::SkipEmptyParts);
    m_clientOutputTail = m_clientOutputTail.mid(lastNewline + 1);

    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (!isConnectionInfoLine(line)) {
            qDebug().noquote() << "[trusttunnel]:" << line;
        }
        if (m_stage == Stage::StartingClient && line.contains(QLatin1String(amnezia::protocols::trustTunnel::connectedLogLine))) {
            m_connectTimer.stop();
            m_stage = Stage::StartingTun2Socks;
            if (ErrorCode e = startTun2Socks(); e != ErrorCode::NoError) {
                fail(e);
                return;
            }
        }
    }
}

ErrorCode TrustTunnelProtocol::startTun2Socks()
{
    m_tun2socksProcess = IpcClient::CreatePrivilegedProcess();
    if (!m_tun2socksProcess || !m_tun2socksProcess->waitForSource()) {
        return ErrorCode::AmneziaServiceConnectionFailed;
    }

    const QString proxyUrl =
            QStringLiteral("socks5://%1:%2@127.0.0.1:%3").arg(m_socksUser, m_socksPassword, QString::number(m_socksPort));

    m_tun2socksProcess->setProgram(amnezia::PermittedProcess::Tun2Socks);
    m_tun2socksProcess->setArguments({ QStringLiteral("-device"), QStringLiteral("tun://%1").arg(kTunName),
                                       QStringLiteral("-proxy"), proxyUrl });

    connect(m_tun2socksProcess.data(), &IpcProcessInterfaceReplica::readyReadStandardError, this,
            [this]() {
                if (!m_tun2socksProcess) {
                    return;
                }
                auto err = m_tun2socksProcess->readAllStandardError();
                if (!err.waitForFinished()) {
                    return;
                }
                const QString line = err.returnValue();
                if (!line.contains("[TCP]") && !line.contains("[UDP]")) {
                    qDebug() << "[tun2socks]:" << line;
                }
                if (m_stage == Stage::StartingTun2Socks && line.contains("[STACK] tun://")
                    && line.contains("<-> socks5://")) {
                    if (ErrorCode res = setupRouting(); res != ErrorCode::NoError) {
                        fail(res);
                    } else {
                        m_stage = Stage::Connected;
                        setConnectionState(Vpn::ConnectionState::Connected);
                    }
                }
            },
            Qt::QueuedConnection);

    connect(m_tun2socksProcess.data(), &IpcProcessInterfaceReplica::finished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                // The previous tun2socks may not have released the TUN yet.
                bool resourceBusy = false;
                if (m_tun2socksProcess) {
                    auto out = m_tun2socksProcess->readAllStandardOutput();
                    resourceBusy = out.waitForFinished() && out.returnValue().contains("resource busy");
                }
                if (resourceBusy && m_tun2socksRetryCount < maxTun2SocksRetries) {
                    ++m_tun2socksRetryCount;
                    qWarning() << "TrustTunnel: TUN busy, retrying tun2socks" << m_tun2socksRetryCount;
                    QTimer::singleShot(tun2socksRetryDelayMs, this, [this]() {
                        if (ErrorCode e = startTun2Socks(); e != ErrorCode::NoError) {
                            fail(e);
                        }
                    });
                    return;
                }
                m_tun2socksRetryCount = 0;
                qCritical() << "TrustTunnel: tun2socks exited, code" << exitCode
                            << (exitStatus == QProcess::CrashExit ? "(crash)" : "");
                fail(ErrorCode::Tun2SockExecutableCrashed);
            },
            Qt::QueuedConnection);

    m_tun2socksProcess->start();
    return ErrorCode::NoError;
}

ErrorCode TrustTunnelProtocol::setupRouting()
{
    return IpcClient::withInterface(
            [this](QSharedPointer<IpcInterfaceReplica> iface) -> ErrorCode {
#ifdef Q_OS_WIN
                const int inetAdapterIndex = NetworkUtilities::AdapterIndexTo(QHostAddress(m_remoteAddress));
#endif
                auto createTun = iface->createTun(kTunName, amnezia::protocols::trustTunnel::defaultLocalAddr);
                if (!createTun.waitForFinished() || !createTun.returnValue()) {
                    qCritical() << "TrustTunnel: failed to assign the TUN address";
                    return ErrorCode::InternalError;
                }

                auto updateResolvers = iface->updateResolvers(kTunName, m_dnsServers);
                if (!updateResolvers.waitForFinished() || !updateResolvers.returnValue()) {
                    qCritical() << "TrustTunnel: failed to set DNS resolvers for the TUN";
                    return ErrorCode::InternalError;
                }

#ifdef Q_OS_WIN
                int vpnAdapterIndex = -1;
                for (const QNetworkInterface &netInterface : QNetworkInterface::allInterfaces()) {
                    for (const QNetworkAddressEntry &address : netInterface.addressEntries()) {
                        if (m_vpnLocalAddress == address.ip().toString()) {
                            vpnAdapterIndex = netInterface.index();
                        }
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
                        auto enableKillSwitch = iface->enableKillSwitch(config, vpnAdapterIndex);
                        if (!enableKillSwitch.waitForFinished() || !enableKillSwitch.returnValue()) {
                            qCritical() << "TrustTunnel: failed to enable the kill switch";
                            return ErrorCode::InternalError;
                        }
                        // trusttunnel_client's sockets are not marked like
                        // Xray's, so every endpoint address needs its own
                        // allowance, not just vpnServer.
                        auto allow = iface->addKillSwitchAllowedRange(m_endpointRoutes);
                        if (!allow.waitForFinished() || !allow.returnValue()) {
                            qCritical() << "TrustTunnel: failed to allow the endpoint through the kill switch";
                            return ErrorCode::InternalError;
                        }
                    } else {
                        qWarning() << "TrustTunnel: no VPN adapter index; kill switch disabled";
                    }
                }

                if (m_routeMode == amnezia::RouteMode::VpnAllSites) {
                    static const QStringList subnets = { "1.0.0.0/8",  "2.0.0.0/7",  "4.0.0.0/6",  "8.0.0.0/5",
                                                         "16.0.0.0/4", "32.0.0.0/3", "64.0.0.0/2", "128.0.0.0/1" };
                    auto routeAddList = iface->routeAddList(m_vpnGateway, subnets);
                    if (!routeAddList.waitForFinished() || routeAddList.returnValue() != subnets.count()) {
                        qCritical() << "TrustTunnel: failed to set routes for the TUN";
                        return ErrorCode::InternalError;
                    }
                }

                auto stopRoutingIpv6 = iface->StopRoutingIpv6();
                if (!stopRoutingIpv6.waitForFinished() || !stopRoutingIpv6.returnValue()) {
                    qCritical() << "TrustTunnel: failed to disable IPv6 routing";
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
                        qCritical() << "TrustTunnel: failed to enable peer traffic";
                        return ErrorCode::InternalError;
                    }
                } else {
                    qWarning() << "TrustTunnel: no adapter indexes; split tunnelling disabled";
                }
#endif
                return ErrorCode::NoError;
            },
            []() { return ErrorCode::AmneziaServiceConnectionFailed; });
}

void TrustTunnelProtocol::fail(ErrorCode code)
{
    if (m_stage == Stage::Idle) {
        return;
    }
    stop();
    setLastError(code);
}

void TrustTunnelProtocol::stopProcess(QSharedPointer<IpcProcessInterfaceReplica> &process, bool graceful)
{
    if (!process) {
        return;
    }
    process->blockSignals(true);
#ifndef Q_OS_WIN
    if (graceful) {
        // trusttunnel_client and tun2socks both shut down cleanly on SIGTERM.
        process->terminate();
        auto finished = process->waitForFinished(1000);
        if (!finished.waitForFinished() || !finished.returnValue()) {
            process->kill();
        }
    } else {
        process->kill();
    }
#else
    // terminate() posts WM_CLOSE, which console programs never see.
    Q_UNUSED(graceful)
    process->kill();
#endif
    process->close();
    process.reset();
}

void TrustTunnelProtocol::stop()
{
    qDebug() << "TrustTunnelProtocol::stop()";
    const bool wasActive = m_stage != Stage::Idle;
    m_stage = Stage::Idle;
    m_connectTimer.stop();

    stopProcess(m_tun2socksProcess, true);
    stopProcess(m_clientProcess, true);

    if (wasActive) {
        IpcClient::withInterface([this](QSharedPointer<IpcInterfaceReplica> iface) {
            auto disableKillSwitch = iface->disableKillSwitch();
            if (!disableKillSwitch.waitForFinished() || !disableKillSwitch.returnValue()) {
                qWarning() << "TrustTunnel: failed to disable the kill switch";
            }

            auto startRoutingIpv6 = iface->StartRoutingIpv6();
            if (!startRoutingIpv6.waitForFinished() || !startRoutingIpv6.returnValue()) {
                qWarning() << "TrustTunnel: failed to restore IPv6 routing";
            }

            auto restoreResolvers = iface->restoreResolvers();
            if (!restoreResolvers.waitForFinished() || !restoreResolvers.returnValue()) {
                qWarning() << "TrustTunnel: failed to restore resolvers";
            }

            auto deleteTun = iface->deleteTun(kTunName);
            if (!deleteTun.waitForFinished() || !deleteTun.returnValue()) {
                qWarning() << "TrustTunnel: failed to delete the TUN";
            }

            if (m_endpointRoutesAdded) {
                auto routes = iface->routeDeleteList(m_routeGateway, m_endpointRoutes);
                if (!routes.waitForFinished() || !routes.returnValue()) {
                    qWarning() << "TrustTunnel: failed to remove the endpoint routes";
                }
                m_endpointRoutesAdded = false;
            }

            // The TOML holds the endpoint credentials; it lives only while connected.
            auto removeConfig = iface->trustTunnelRemoveConfig();
            if (!removeConfig.waitForFinished() || !removeConfig.returnValue()) {
                qWarning() << "TrustTunnel: failed to remove the client config";
            }
        });
    }

    setConnectionState(Vpn::ConnectionState::Disconnected);
}
