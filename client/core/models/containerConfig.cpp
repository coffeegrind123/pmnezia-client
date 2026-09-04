#include "containerConfig.h"

#include <QJsonDocument>

#include "core/utils/containerEnum.h"
#include "core/utils/containers/containerUtils.h"
#include "core/utils/protocolEnum.h"
#include "core/utils/protocolEnum.h"
#include "core/protocols/protocolUtils.h"
#include "core/utils/constants/configKeys.h"
#include "core/utils/constants/protocolConstants.h"

namespace amnezia
{

using namespace ContainerEnumNS;
using namespace ProtocolEnumNS;
using namespace ProtocolUtils;

Proto ContainerConfig::getProtocolType() const
{
    return ContainerUtils::defaultProtocol(container);
}

QJsonObject ContainerConfig::toJson() const
{
    QJsonObject obj;
    
    obj[configKey::container] = ContainerUtils::containerToString(container);
    
    Proto protoType = getProtocolType();
    QString protoName = ProtocolUtils::protoToString(protoType);
    
    obj[protoName] = protocolConfig.toJson();
    
    return obj;
}

ContainerConfig ContainerConfig::fromJson(const QJsonObject& json)
{
    ContainerConfig config;
    
    QString containerStr = json.value(configKey::container).toString();
    config.container = ContainerUtils::containerFromString(containerStr);
    
    Proto protoType = ContainerUtils::defaultProtocol(config.container);
    QString protoName = ProtocolUtils::protoToString(protoType);
    
    QJsonObject protoJson = json.value(protoName).toObject();
    
    config.protocolConfig = ProtocolConfig::fromJson(protoJson, protoType);
    
    return config;
}

AwgProtocolConfig* ContainerConfig::getAwgProtocolConfig()
{
    return protocolConfig.as<AwgProtocolConfig>();
}

const AwgProtocolConfig* ContainerConfig::getAwgProtocolConfig() const
{
    return protocolConfig.as<AwgProtocolConfig>();
}

WireGuardProtocolConfig* ContainerConfig::getWireGuardProtocolConfig()
{
    return protocolConfig.as<WireGuardProtocolConfig>();
}

const WireGuardProtocolConfig* ContainerConfig::getWireGuardProtocolConfig() const
{
    return protocolConfig.as<WireGuardProtocolConfig>();
}

XrayProtocolConfig* ContainerConfig::getXrayProtocolConfig()
{
    return protocolConfig.as<XrayProtocolConfig>();
}

const XrayProtocolConfig* ContainerConfig::getXrayProtocolConfig() const
{
    return protocolConfig.as<XrayProtocolConfig>();
}

MasterDnsVpnProtocolConfig* ContainerConfig::getMasterDnsVpnProtocolConfig()
{
    return protocolConfig.as<MasterDnsVpnProtocolConfig>();
}

const MasterDnsVpnProtocolConfig* ContainerConfig::getMasterDnsVpnProtocolConfig() const
{
    return protocolConfig.as<MasterDnsVpnProtocolConfig>();
}

QqDnsProtocolConfig* ContainerConfig::getQqDnsProtocolConfig()
{
    return protocolConfig.as<QqDnsProtocolConfig>();
}

const QqDnsProtocolConfig* ContainerConfig::getQqDnsProtocolConfig() const
{
    return protocolConfig.as<QqDnsProtocolConfig>();
}

} // namespace amnezia

