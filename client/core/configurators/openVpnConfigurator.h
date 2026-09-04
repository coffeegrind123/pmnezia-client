#ifndef OPENVPN_CONFIGURATOR_H
#define OPENVPN_CONFIGURATOR_H

#include <QObject>

#include "configuratorBase.h"

class OpenVpnConfigurator : public ConfiguratorBase
{
    Q_OBJECT
public:
    explicit OpenVpnConfigurator(QObject *parent = nullptr);

    amnezia::ProtocolConfig processConfigWithLocalSettings(const amnezia::ConnectionSettings &settings,
                                                           amnezia::ProtocolConfig protocolConfig) override;
    amnezia::ProtocolConfig processConfigWithExportSettings(const amnezia::ExportSettings &settings,
                                                            amnezia::ProtocolConfig protocolConfig) override;
};

#endif // OPENVPN_CONFIGURATOR_H
