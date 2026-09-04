#ifndef SERVERDESCRIPTION_H
#define SERVERDESCRIPTION_H

#include <QString>

#include "core/utils/containerEnum.h"
#include "core/utils/selfhosted/sshSession.h"
#include "core/models/selfhosted/selfHostedAdminServerConfig.h"
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

    ServerCredentials selfHostedSshCredentials;
    bool hasWriteAccess = false;

    bool primaryDnsIsAmnezia = false;
    DockerContainer defaultContainer = DockerContainer::None;
    bool hasInstalledVpnContainers = false;

    QString collapsedServerDescription;
    QString expandedServerDescription;
};

ServerDescription buildServerDescription(const SelfHostedAdminServerConfig &server, bool isAmneziaDnsEnabled);
ServerDescription buildServerDescription(const SelfHostedUserServerConfig &server, bool isAmneziaDnsEnabled);
ServerDescription buildServerDescription(const NativeServerConfig &server, bool isAmneziaDnsEnabled);

} // namespace amnezia

#endif
