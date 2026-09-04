#include "serversUiController.h"

#include "core/utils/containerEnum.h"
#include "core/utils/containers/containerUtils.h"
#include "core/utils/protocolEnum.h"
#include "core/models/protocolConfig.h"
#include "core/models/containerConfig.h"
#include "core/models/protocols/awgProtocolConfig.h"
#include "core/utils/constants/protocolConstants.h"
#include "core/utils/networkUtilities.h"

using namespace amnezia;

namespace {
int rowForServerId(const QVector<ServerDescription> &list, const QString &serverId)
{
    if (serverId.isEmpty()) {
        return -1;
    }
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i).serverId == serverId) {
            return i;
        }
    }
    return -1;
}

const ServerDescription &emptyServerDescription()
{
    static const ServerDescription s_emptyDescription;
    return s_emptyDescription;
}
} // namespace
ServersUiController::ServersUiController(ServersController* serversController,
                                         SettingsController* settingsController,
                                         ServersModel* serversModel,
                                         ContainersModel* containersModel,
                                         ContainersModel* defaultServerContainersModel,
                                         ProtocolsModel* protocolsModel,
                                         AwgConfigModel* awgConfigModel,
                                         WireGuardConfigModel* wireGuardConfigModel,
                                         OpenVpnConfigModel* openVpnConfigModel,
                                         XrayConfigModel* xrayConfigModel,
                                         MasterDnsVpnConfigModel* masterDnsVpnConfigModel,
                                         QqDnsConfigModel* qqDnsConfigModel,
#ifdef Q_OS_WINDOWS
                                         Ikev2ConfigModel* ikev2ConfigModel,
#endif
                                         QObject *parent)
    : QObject(parent),
      m_serversController(serversController),
      m_settingsController(settingsController),
      m_serversModel(serversModel),
      m_containersModel(containersModel),
      m_defaultServerContainersModel(defaultServerContainersModel),
      m_protocolsModel(protocolsModel),
      m_awgConfigModel(awgConfigModel),
      m_wireGuardConfigModel(wireGuardConfigModel),
      m_openVpnConfigModel(openVpnConfigModel),
      m_xrayConfigModel(xrayConfigModel),
      m_masterDnsVpnConfigModel(masterDnsVpnConfigModel),
      m_qqDnsConfigModel(qqDnsConfigModel)
#ifdef Q_OS_WINDOWS
      , m_ikev2ConfigModel(ikev2ConfigModel)
#endif
{
}

void ServersUiController::removeServer(const QString &serverId)
{
    if (serverId.isEmpty()) {
        return;
    }
    const QString name = serverName(serverId);
    m_serversController->removeServer(serverId);
    updateModel();
    emit removeServerFinished(tr("Server '%1' was removed").arg(name));
}

void ServersUiController::updateProtocols(const QString &serverId, int containerIndex)
{
    const DockerContainer container = static_cast<DockerContainer>(containerIndex);
    ContainerConfig containerConfig = m_serversController->getContainerConfig(serverId, container);
    containerConfig.container = container;
    m_protocolsModel->updateModel(containerConfig);
}

void ServersUiController::openClientSettings(const QString &serverId, int containerIndex, int protocolIndex)
{
    updateProtocolConfigModel(serverId, containerIndex, protocolIndex);
}

void ServersUiController::updateClientConfig(const QString &serverId, int containerIndex, int protocolIndex,
                                             bool closePage)
{
    const DockerContainer container = static_cast<DockerContainer>(containerIndex);

    ContainerConfig containerConfig;
    if (!buildContainerConfigFromModel(containerIndex, protocolIndex, containerConfig)) {
        return;
    }

    const ErrorCode errorCode = m_serversController->updateClientConfig(serverId, container, containerConfig);
    if (errorCode != ErrorCode::NoError) {
        emit updateContainerErrorOccurred(errorCode);
        return;
    }

    ContainerConfig updatedConfig = m_serversController->getContainerConfig(serverId, container);
    updatedConfig.container = container;
    m_protocolsModel->updateModel(updatedConfig);
    updateProtocolConfigModel(serverId, containerIndex, protocolIndex);
    updateModel();

    emit updateContainerFinished(tr("Settings updated successfully"), closePage);
}

QRegularExpression ServersUiController::ipAddressRegExp()
{
    return NetworkUtilities::ipAddressRegExp();
}

bool ServersUiController::buildContainerConfigFromModel(int containerIndex, int protocolIndex,
                                                        ContainerConfig &containerConfig)
{
    containerConfig.container = static_cast<DockerContainer>(containerIndex);

    switch (static_cast<Proto>(protocolIndex)) {
    case Proto::Awg: containerConfig.protocolConfig = m_awgConfigModel->getProtocolConfig(); break;
    case Proto::WireGuard: containerConfig.protocolConfig = m_wireGuardConfigModel->getProtocolConfig(); break;
    case Proto::OpenVpn: containerConfig.protocolConfig = m_openVpnConfigModel->getProtocolConfig(); break;
    case Proto::Xray:
    case Proto::SSXray: containerConfig.protocolConfig = m_xrayConfigModel->getProtocolConfig(); break;
    case Proto::MasterDnsVpn: containerConfig.protocolConfig = m_masterDnsVpnConfigModel->getProtocolConfig(); break;
    case Proto::QqDns: containerConfig.protocolConfig = m_qqDnsConfigModel->getProtocolConfig(); break;
#ifdef Q_OS_WINDOWS
    case Proto::Ikev2: containerConfig.protocolConfig = m_ikev2ConfigModel->getProtocolConfig(); break;
#endif
    default: return false;
    }
    return true;
}

void ServersUiController::updateProtocolConfigModel(const QString &serverId, int containerIndex, int protocolIndex)
{
    const DockerContainer container = static_cast<DockerContainer>(containerIndex);
    ContainerConfig containerConfig = m_serversController->getContainerConfig(serverId, container);
    containerConfig.container = container;

    auto updateIfPresent = [&](auto* model, auto* config) {
        if (model && config) model->updateModel(container, *config);
    };

    switch (static_cast<Proto>(protocolIndex)) {
    case Proto::Awg: updateIfPresent(m_awgConfigModel, containerConfig.getAwgProtocolConfig()); break;
    case Proto::WireGuard: updateIfPresent(m_wireGuardConfigModel, containerConfig.getWireGuardProtocolConfig()); break;
    case Proto::OpenVpn: updateIfPresent(m_openVpnConfigModel, containerConfig.getOpenVpnProtocolConfig()); break;
    case Proto::Xray:
    case Proto::SSXray: updateIfPresent(m_xrayConfigModel, containerConfig.getXrayProtocolConfig()); break;
    case Proto::MasterDnsVpn:
        updateIfPresent(m_masterDnsVpnConfigModel, containerConfig.getMasterDnsVpnProtocolConfig());
        break;
    case Proto::QqDns: updateIfPresent(m_qqDnsConfigModel, containerConfig.getQqDnsProtocolConfig()); break;
#ifdef Q_OS_WINDOWS
    case Proto::Ikev2: updateIfPresent(m_ikev2ConfigModel, containerConfig.getIkev2ProtocolConfig()); break;
#endif
    default: break;
    }
}

void ServersUiController::removeServerAtIndex(int index)
{
    const QString serverId = getServerId(index);
    if (!serverId.isEmpty()) {
        removeServer(serverId);
    }
}

void ServersUiController::setDefaultServerAtIndex(int index)
{
    const QString serverId = getServerId(index);
    if (!serverId.isEmpty()) {
        setDefaultServer(serverId);
    }
}

void ServersUiController::setDefaultContainerAtIndex(int index, int containerIndex)
{
    const QString serverId = getServerId(index);
    if (!serverId.isEmpty()) {
        setDefaultContainer(serverId, containerIndex);
    }
}

void ServersUiController::editServerName(const QString &serverId, const QString &name)
{
    if (serverId.isEmpty()) {
        return;
    }

    if (!m_serversController->renameServer(serverId, name)) {
        emit errorOccurred(tr("Legacy API v1 configs are no longer supported. Remove this server to continue."));
        emit finished(tr("Use the remove action to delete this legacy config."));
        return;
    }
    updateModel();
}

void ServersUiController::setDefaultServer(const QString &serverId)
{
    if (serverId.isEmpty()) {
        return;
    }
    m_serversController->setDefaultServer(serverId);
}

void ServersUiController::setDefaultContainer(const QString &serverId, int containerIndex)
{
    if (serverId.isEmpty()) {
        return;
    }
    auto container = static_cast<DockerContainer>(containerIndex);
    m_serversController->setDefaultContainer(serverId, container);
    updateModel();
}

void ServersUiController::toggleAmneziaDns(bool enabled)
{
    m_settingsController->toggleAmneziaDns(enabled);
    updateModel();
}

void ServersUiController::onDefaultServerChanged(const QString &defaultServerId)
{
    m_serversModel->setDefaultServerId(defaultServerId);
    updateDefaultServerContainersModel();

    emit defaultServerIdChanged(defaultServerId);
}

void ServersUiController::updateModel()
{
    QVector<ServerDescription> descriptions =
        m_serversController->buildServerDescriptions();

    const QString defaultServerId = m_serversController->getDefaultServerId();
    m_orderedServerDescriptions = descriptions;

    if (m_orderedServerDescriptions.isEmpty()) {
        if (!m_processedServerId.isEmpty()) {
            setProcessedServerId(QString());
        }
    } else if (!m_processedServerId.isEmpty()) {
        const int row = rowForServerId(m_orderedServerDescriptions, m_processedServerId);
        if (row < 0) {
            setProcessedServerId(QString());
        }
    }

    m_serversModel->updateModel(m_orderedServerDescriptions, defaultServerId);

    if (!m_processedServerId.isEmpty()) {
        updateContainersModel();
    }
    updateDefaultServerContainersModel();

    emit defaultServerIdChanged(defaultServerId);
}

QString ServersUiController::getDefaultServerId() const
{
    return m_serversController->getDefaultServerId();
}

QString ServersUiController::getDefaultServerName() const
{
    return serverName(getDefaultServerId());
}

QString ServersUiController::getDefaultServerDefaultContainerName() const
{
    const auto &description = serverDescriptionById(getDefaultServerId());
    if (description.serverId.isEmpty()) {
        return QString();
    }
    return ContainerUtils::containerHumanNames().value(description.defaultContainer);
}

QString ServersUiController::getDefaultServerDescriptionCollapsed() const
{
    return serverDescriptionById(getDefaultServerId()).collapsedServerDescription;
}

QString ServersUiController::getDefaultServerDescriptionExpanded() const
{
    return serverDescriptionById(getDefaultServerId()).expandedServerDescription;
}

bool ServersUiController::isDefaultServerDefaultContainerHasSplitTunneling() const
{
    const QString defaultServerId = m_serversController->getDefaultServerId();
    const DockerContainer defaultContainer = m_serversController->getDefaultContainer(defaultServerId);
    const ContainerConfig containerConfig = m_serversController->getContainerConfig(defaultServerId, defaultContainer);
    
    if (defaultContainer == DockerContainer::Awg || defaultContainer == DockerContainer::WireGuard) {
        auto hasSplitTunnelingFromAllowedIps = [](const QStringList& allowedIps, const QString& nativeConfig) -> bool {
            bool hasSplitTunneling = !allowedIps.isEmpty() && !allowedIps.contains("0.0.0.0/0");
            if (!hasSplitTunneling && !nativeConfig.isEmpty()) {
                hasSplitTunneling = nativeConfig.contains("AllowedIPs") 
                    && !nativeConfig.contains("AllowedIPs = 0.0.0.0/0, ::/0");
            }
            return hasSplitTunneling;
        };
        
        if (defaultContainer == DockerContainer::Awg) {
            if (const auto* awgConfig = containerConfig.getAwgProtocolConfig()) {
                if (awgConfig->hasClientConfig()) {
                    return hasSplitTunnelingFromAllowedIps(
                        awgConfig->clientConfig->allowedIps,
                        awgConfig->clientConfig->nativeConfig
                    );
                }
            }
        } else if (defaultContainer == DockerContainer::WireGuard) {
            if (const auto* wgConfig = containerConfig.getWireGuardProtocolConfig()) {
                if (wgConfig->hasClientConfig()) {
                    return hasSplitTunnelingFromAllowedIps(
                        wgConfig->clientConfig->allowedIps,
                        wgConfig->clientConfig->nativeConfig
                    );
                }
            }
        }
        return false;
    } else if (defaultContainer == DockerContainer::OpenVpn) {
        if (const auto* ovpnConfig = containerConfig.getOpenVpnProtocolConfig()) {
            if (ovpnConfig->hasClientConfig()) {
                return !ovpnConfig->clientConfig->nativeConfig.isEmpty() 
                    && !ovpnConfig->clientConfig->nativeConfig.contains("redirect-gateway");
            }
        }
    }
    return false;
}

QString ServersUiController::serverName(const QString &serverId) const
{
    return serverDescriptionById(serverId).serverName;
}

QString ServersUiController::serverHostName(const QString &serverId) const
{
    return serverDescriptionById(serverId).hostName;
}

int ServersUiController::serverDefaultContainer(const QString &serverId) const
{
    const auto &description = serverDescriptionById(serverId);
    return description.serverId.isEmpty() ? -1 : static_cast<int>(description.defaultContainer);
}

bool ServersUiController::serverHasInstalledContainers(const QString &serverId) const
{
    return serverDescriptionById(serverId).hasInstalledVpnContainers;
}

int ServersUiController::getProcessedContainerIndex() const
{
    return m_processedContainerIndex;
}

void ServersUiController::setProcessedContainerIndex(int index)
{
    if (m_processedContainerIndex != index) {
        m_processedContainerIndex = index;
        m_containersModel->setProcessedContainerIndex(index);
        emit processedContainerIndexChanged(m_processedContainerIndex);
    }
}

QString ServersUiController::getProcessedServerId() const
{
    return m_processedServerId;
}

void ServersUiController::setProcessedServerId(const QString &serverId)
{
    const int newIndex = serverId.isEmpty() ? -1 : serverIndexForId(serverId);
    const QString normalizedServerId = newIndex >= 0 ? serverId : QString();

    if (m_processedServerId != normalizedServerId) {
        m_processedServerId = normalizedServerId;

        if (newIndex >= 0) {
            updateContainersModel();
        }

        emit processedServerIdChanged(m_processedServerId);
    }
}

bool ServersUiController::isDefaultServerCurrentlyProcessed() const
{
    return m_serversController->getDefaultServerId() == m_processedServerId;
}

bool ServersUiController::serverHasOutdatedAwgContainer(const QString &serverId) const
{
    const QMap<DockerContainer, ContainerConfig> containers = m_serversController->getServerContainersMap(serverId);
    for (DockerContainer container : { DockerContainer::Awg, DockerContainer::Awg2 }) {
        if (!containers.contains(container)) {
            continue;
        }
        if (const auto* awgConfig = containers.value(container).getAwgProtocolConfig()) {
            if (awgConfig->serverConfig.protocolVersion != protocols::awg::awgV3) {
                return true;
            }
        }
    }
    return false;
}

bool ServersUiController::defaultServerHasOutdatedAwgContainer() const
{
    return serverHasOutdatedAwgContainer(getDefaultServerId());
}

bool ServersUiController::isContainerOutdatedAwg(int containerIndex) const
{
    DockerContainer container = static_cast<DockerContainer>(containerIndex);
    if (!ContainerUtils::isAwgContainer(container)) {
        return false;
    }

    const QMap<DockerContainer, ContainerConfig> containers = m_serversController->getServerContainersMap(m_processedServerId);
    if (!containers.contains(container)) {
        return false;
    }
    if (const auto* awgConfig = containers.value(container).getAwgProtocolConfig()) {
        return awgConfig->serverConfig.protocolVersion != protocols::awg::awgV3;
    }
    return false;
}

bool ServersUiController::isProcessedContainerOutdatedAwg() const
{
    return isContainerOutdatedAwg(m_processedContainerIndex);
}

const ServerDescription &ServersUiController::processedServerDescription() const
{
    return serverDescriptionById(m_processedServerId);
}

const ServerDescription &ServersUiController::serverDescriptionById(const QString &serverId) const
{
    for (const auto &description : m_orderedServerDescriptions) {
        if (description.serverId == serverId) {
            return description;
        }
    }
    return emptyServerDescription();
}

QString ServersUiController::getServerId(int index) const
{
    if (index < 0 || index >= m_orderedServerDescriptions.size()) {
        return QString();
    }
    return m_orderedServerDescriptions.at(index).serverId;
}

int ServersUiController::getServerIndexById(const QString &serverId) const
{
    return rowForServerId(m_orderedServerDescriptions, serverId);
}

int ServersUiController::getServersCount() const
{
    return m_orderedServerDescriptions.size();
}

void ServersUiController::updateContainersModel()
{
    if (m_processedServerId.isEmpty()) {
        return;
    }
    const QMap<DockerContainer, ContainerConfig> containers =
            m_serversController->getServerContainersMap(m_processedServerId);
    m_containersModel->updateModel(containers);
}

void ServersUiController::updateDefaultServerContainersModel()
{
    const QString defaultServerId = m_serversController->getDefaultServerId();
    if (defaultServerId.isEmpty()) {
        return;
    }
    const QMap<DockerContainer, ContainerConfig> containers =
            m_serversController->getServerContainersMap(defaultServerId);
    m_defaultServerContainersModel->updateModel(containers);
}

QStringList ServersUiController::getAllInstalledServicesName(int serverIndex) const
{
    QStringList servicesName;
    const QString serverId = getServerId(serverIndex);
    const QMap<DockerContainer, ContainerConfig> containers = m_serversController->getServerContainersMap(serverId);

    for (auto it = containers.begin(); it != containers.end(); ++it) {
        DockerContainer container = it.key();
        if (ContainerUtils::containerService(container) == ServiceType::Other) {
            if (container == DockerContainer::Dns) {
                servicesName.append("DNS");
            } else if (container == DockerContainer::Sftp) {
                servicesName.append("SFTP");
            } else if (container == DockerContainer::TorWebSite) {
                servicesName.append("TOR");
            } else if (container == DockerContainer::Socks5Proxy) {
                servicesName.append("SOCKS5");
            } else if (container == DockerContainer::MtProxy) {
                servicesName.append("MTProxy");
            } else if (container == DockerContainer::Telemt) {
                servicesName.append("Telemt");
            } else if (container == DockerContainer::TProxy) {
                servicesName.append("TProxy");
            }
        }
    }
    servicesName.sort();
    return servicesName;
}

int ServersUiController::serverIndexForId(const QString &serverId) const
{
    return rowForServerId(m_orderedServerDescriptions, serverId);
}

