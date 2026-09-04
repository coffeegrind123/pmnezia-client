#include "containersModel.h"

#include <QJsonArray>

#include "core/models/protocolConfig.h"

using namespace amnezia;

ContainersModel::ContainersModel(QObject *parent) : QAbstractListModel(parent)
{
}

int ContainersModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return ContainerUtils::allContainers().size();
}

QVariant ContainersModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= ContainerUtils::allContainers().size()) {
        return QVariant();
    }

    DockerContainer container = ContainerUtils::allContainers().at(index.row());
    bool isThirdPartyConfig = false;
    if (m_containers.contains(container)) {
        const ContainerConfig& config = m_containers.value(container);
        isThirdPartyConfig = config.protocolConfig.isThirdPartyConfig();
    }

    switch (role) {
    case NameRole: {
        if (container == DockerContainer::Awg && !isThirdPartyConfig) {
            return "AmneziaWG Legacy";
        }
        return ContainerUtils::containerHumanNames().value(container);
    }
    case DescriptionRole: {
        if (container == DockerContainer::Awg && !isThirdPartyConfig) {
            return QObject::tr("AmneziaWG is a special protocol from Amnezia based on WireGuard. "
                           "It provides high connection speed and ensures stable operation even in the most challenging network conditions.");
        }

        return ContainerUtils::containerDescriptions().value(container);
    }
    case DetailedDescriptionRole: return ContainerUtils::containerDetailedDescriptions().value(container);
    case ConfigRole: {
        if (container == DockerContainer::None) {
            return QJsonObject();
        }
        if (m_containers.contains(container)) {
            return m_containers.value(container).toJson();
        }
        return QJsonObject();
    }
    case IsThirdPartyConfigRole: return isThirdPartyConfig;
    case ServiceTypeRole: return ContainerUtils::containerService(container);
    case DockerContainerRole: return container;
    case ContainerStringRole: return ContainerUtils::containerToString(container);
    case IsEasySetupContainerRole: return ContainerUtils::isEasySetupContainer(container);
    case EasySetupOrderRole: return ContainerUtils::easySetupOrder(container);
    case IsInstalledRole: return m_containers.contains(container);
    case IsCurrentlyProcessedRole: return container == static_cast<DockerContainer>(m_processedContainerIndex);
    case IsSupportedRole: return ContainerUtils::isSupportedByCurrentPlatform(container);
    case IsVpnContainerRole: return ContainerUtils::containerService(container) == ServiceType::Vpn;
    case InstallPageOrderRole: return ContainerUtils::installPageOrder(container);
    }

    return QVariant();
}

QVariant ContainersModel::data(const int index, int role) const
{
    QModelIndex modelIndex = this->index(index);
    return data(modelIndex, role);
}

void ContainersModel::updateModel(const QMap<DockerContainer, ContainerConfig> &containers)
{
    beginResetModel();
    m_containers = containers;
    endResetModel();
}

void ContainersModel::setProcessedContainerIndex(int index)
{
    m_processedContainerIndex = index;
}

QString ContainersModel::getProcessedContainerName()
{
    return ContainerUtils::containerHumanNames().value(static_cast<DockerContainer>(m_processedContainerIndex));
}

QJsonObject ContainersModel::getContainerConfig(const int containerIndex)
{
    return qvariant_cast<QJsonObject>(data(index(containerIndex), ConfigRole));
}

bool ContainersModel::isSupportedByCurrentPlatform(const int containerIndex)
{
    return qvariant_cast<bool>(data(index(containerIndex), IsSupportedRole));
}

bool ContainersModel::hasInstalledProtocols()
{
    for (const auto &container : m_containers.keys()) {
        if (ContainerUtils::containerService(container) == ServiceType::Vpn) {
            return true;
        }
    }
    return false;
}

QHash<int, QByteArray> ContainersModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[DescriptionRole] = "description";
    roles[DetailedDescriptionRole] = "detailedDescription";
    roles[ServiceTypeRole] = "serviceType";
    roles[DockerContainerRole] = "dockerContainer";
    roles[ContainerStringRole] = "containerString";
    roles[ConfigRole] = "config";
    roles[IsThirdPartyConfigRole] = "isThirdPartyConfig";

    roles[IsEasySetupContainerRole] = "isEasySetupContainer";
    roles[EasySetupOrderRole] = "easySetupOrder";

    roles[IsInstalledRole] = "isInstalled";
    roles[IsCurrentlyProcessedRole] = "isCurrentlyProcessed";
    roles[IsSupportedRole] = "isSupported";
    roles[InstallPageOrderRole] = "installPageOrder";
    
    roles[IsVpnContainerRole] = "isVpnContainer";
    return roles;
}
