#ifndef CONFIGURATORBASE_H
#define CONFIGURATORBASE_H

#include <QObject>
#include <QScopedPointer>

#include "core/utils/containerEnum.h"
#include "core/utils/containers/containerUtils.h"
#include "core/utils/protocolEnum.h"
#include "core/utils/errorCodes.h"
#include "core/utils/routeModes.h"
#include "core/utils/commonStructs.h"
#include "core/models/containerConfig.h"
#include "core/models/protocolConfig.h"

// Post-processes a stored protocol config on its way out to the VPN backend or
// to an exported file. Configs arrive fully formed from the server that issued
// them; nothing here contacts a server.
class ConfiguratorBase : public QObject
{
    Q_OBJECT
public:
    explicit ConfiguratorBase(QObject *parent = nullptr);

    // Returns a null pointer for protocols that need no post-processing; every
    // caller must handle that.
    static QScopedPointer<ConfiguratorBase> create(amnezia::Proto protocol);

    virtual amnezia::ProtocolConfig processConfigWithLocalSettings(const amnezia::ConnectionSettings &settings,
                                                                   amnezia::ProtocolConfig protocolConfig);
    virtual amnezia::ProtocolConfig processConfigWithExportSettings(const amnezia::ExportSettings &settings,
                                                                     amnezia::ProtocolConfig protocolConfig);

protected:
    void applyDnsToNativeConfig(const amnezia::DnsSettings &dns, amnezia::ProtocolConfig &protocolConfig);
};

#endif // CONFIGURATORBASE_H
