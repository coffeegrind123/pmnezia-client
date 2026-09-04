#include "configuratorBase.h"

#include "core/configurators/xrayConfigurator.h"

using namespace amnezia;

ConfiguratorBase::ConfiguratorBase(QObject *parent)
    : QObject { parent }
{
}

QScopedPointer<ConfiguratorBase> ConfiguratorBase::create(Proto protocol)
{
    switch (protocol) {
    case Proto::Xray: return QScopedPointer<ConfiguratorBase>(new XrayConfigurator());
    // WireGuard and AmneziaWG need only the shared DNS substitution.
    case Proto::WireGuard:
    case Proto::Awg: return QScopedPointer<ConfiguratorBase>(new ConfiguratorBase());
    // MasterDnsVPN and QQ-DNS carry a complete config; nothing to post-process.
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
