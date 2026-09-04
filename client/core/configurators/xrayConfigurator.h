#ifndef XRAY_CONFIGURATOR_H
#define XRAY_CONFIGURATOR_H

#include <QObject>

#include "configuratorBase.h"

class XrayConfigurator : public ConfiguratorBase
{
    Q_OBJECT
public:
    explicit XrayConfigurator(QObject *parent = nullptr);

    amnezia::ProtocolConfig processConfigWithLocalSettings(const amnezia::ConnectionSettings &settings,
                                                           amnezia::ProtocolConfig protocolConfig) override;
};

#endif // XRAY_CONFIGURATOR_H
