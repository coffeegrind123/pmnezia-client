#include "configuratorBase.h"

#include "core/configurators/openVpnConfigurator.h"
#include "core/configurators/xrayConfigurator.h"

using namespace amnezia;

ConfiguratorBase::ConfiguratorBase(QObject *parent)
    : QObject { parent }
{
}

QScopedPointer<ConfiguratorBase> ConfiguratorBase::create(Proto protocol)
{
    switch (protocol) {
    case Proto::OpenVpn: return QScopedPointer<ConfiguratorBase>(new OpenVpnConfigurator());
    case Proto::Xray:
    case Proto::SSXray: return QScopedPointer<ConfiguratorBase>(new XrayConfigurator());
    // WireGuard, AmneziaWG and IKEv2 need only the shared DNS substitution.
    case Proto::WireGuard:
    case Proto::Awg:
    case Proto::Ikev2: return QScopedPointer<ConfiguratorBase>(new ConfiguratorBase());
    default: return QScopedPointer<ConfiguratorBase>();
    }
}

ProtocolConfig ConfiguratorBase::processConfigWithLocalSettings(const ConnectionSettings &settings,
                                                                 ProtocolConfig protocolConfig)
{
    applyDnsToNativeConfig(settings.dns, protocolConfig);
    return protocolConfig;
}

ProtocolConfig ConfiguratorBase::processConfigWithExportSettings(const ExportSettings &settings,
                                                                 ProtocolConfig protocolConfig)
{
    applyDnsToNativeConfig(settings.dns, protocolConfig);
    return protocolConfig;
}

void ConfiguratorBase::applyDnsToNativeConfig(const DnsSettings &dns, ProtocolConfig &protocolConfig)
{
    QString config = protocolConfig.nativeConfig();
    config.replace("$PRIMARY_DNS", dns.primaryDns);
    config.replace("$SECONDARY_DNS", dns.secondaryDns);
    protocolConfig.setNativeConfig(config);
}
