#include "protocolsModel.h"

#include "core/utils/protocolEnum.h"
#include "core/protocols/protocolUtils.h"
#include "core/utils/constants/configKeys.h"
#include "core/utils/constants/protocolConstants.h"
#include "core/utils/containerEnum.h"
#include "core/utils/containers/containerUtils.h"
#include "core/models/protocols/awgProtocolConfig.h"
#include "core/models/protocols/wireGuardProtocolConfig.h"
#include "core/models/protocols/xrayProtocolConfig.h"

using namespace ProtocolUtils;

ProtocolsModel::ProtocolsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ProtocolsModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_containerConfig.container != DockerContainer::None ? 1 : 0;
}

QHash<int, QByteArray> ProtocolsModel::roleNames() const
{
    QHash<int, QByteArray> roles;

    roles[ProtocolNameRole] = "protocolName";
    roles[ClientProtocolPageRole] = "clientProtocolPage";
    roles[ProtocolIndexRole] = "protocolIndex";
    roles[ProtocolStringRole] = "protocolString";
    roles[RawConfigRole] = "rawConfig";
    roles[IsClientProtocolExistsRole] = "isClientProtocolExists";

    return roles;
}

QVariant ProtocolsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return QVariant();
    }

    Proto proto = getProtocolType();
    
    switch (role) {
    case ProtocolNameRole: {
        return ProtocolUtils::protocolHumanNames().value(proto);
    }
    case ClientProtocolPageRole:
        return static_cast<int>(clientProtocolPage(proto));
    case ProtocolIndexRole: return static_cast<int>(proto);
    case ProtocolStringRole: return ProtocolUtils::protoToString(proto);
    case RawConfigRole:
        return getRawConfig();
    case IsClientProtocolExistsRole:
        return isClientProtocolExists();
    }

    return QVariant();
}

void ProtocolsModel::updateModel(const amnezia::ContainerConfig &containerConfig)
{
    beginResetModel();
    m_containerConfig = containerConfig;
    endResetModel();
}

Proto ProtocolsModel::getProtocolType() const
{
    return m_containerConfig.getProtocolType();
}

QString ProtocolsModel::getRawConfig() const
{
    QString configString = m_containerConfig.protocolConfig.nativeConfig();
    
    QStringList lines = configString.replace("\r", "").split("\n");
    QString rawConfig;
    for (const QString &l : lines) {
        rawConfig.append(l + "\n");
    }
    return rawConfig;
}

bool ProtocolsModel::isClientProtocolExists() const
{
    return m_containerConfig.protocolConfig.hasClientConfig() && 
           !m_containerConfig.protocolConfig.nativeConfig().isEmpty();
}

PageLoader::PageEnum ProtocolsModel::clientProtocolPage(Proto protocol) const
{
    switch (protocol) {
    case Proto::WireGuard: return PageLoader::PageEnum::PageProtocolWireGuardClientSettings;
    case Proto::Awg: return PageLoader::PageEnum::PageProtocolAwgClientSettings;
    case Proto::MasterDnsVpn: return PageLoader::PageEnum::PageProtocolMasterDnsVpnSettings;
    case Proto::QqDns: return PageLoader::PageEnum::PageProtocolQqDnsSettings;
    // Everything else is viewed and edited as raw config.
    default: return PageLoader::PageEnum::PageProtocolRaw;
    }
}
