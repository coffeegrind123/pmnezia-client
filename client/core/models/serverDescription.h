#ifndef SERVERDESCRIPTION_H
#define SERVERDESCRIPTION_H

#include <QString>

#include "core/utils/containerEnum.h"
#include "core/models/selfhosted/selfHostedUserServerConfig.h"
#include "core/models/selfhosted/nativeServerConfig.h"

namespace amnezia
{

struct ServerDescription
{
    QString serverId;

    QString serverName;
    QString baseDescription;
    QString hostName;

    bool primaryDnsIsAmnezia = false;
    DockerContainer defaultContainer = DockerContainer::None;
    bool hasInstalledVpnContainers = false;

    QString collapsedServerDescription;
    QString expandedServerDescription;
};

ServerDescription buildServerDescription(const SelfHostedUserServerConfig &server);
ServerDescription buildServerDescription(const NativeServerConfig &server);

} // namespace amnezia

#endif
