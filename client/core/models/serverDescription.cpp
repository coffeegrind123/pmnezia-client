#include "serverDescription.h"

#include <QMap>

#include "core/utils/constants/protocolConstants.h"
#include "core/utils/containers/containerUtils.h"
#include "core/protocols/protocolUtils.h"
#include "core/models/protocols/awgProtocolConfig.h"

using namespace amnezia;

namespace
{

bool computeHasInstalledVpnContainers(const QMap<DockerContainer, ContainerConfig> &containers)
{
    for (auto it = containers.begin(); it != containers.end(); ++it) {
        const DockerContainer container = it.key();
        if (ContainerUtils::containerService(container) == ServiceType::Vpn) {
            return true;
        }
    }
    return false;
}

template <typename T>
ServerDescription buildBaseDescription(const T &server)
{
    ServerDescription row;
    row.hostName = server.hostName;
    row.defaultContainer = server.defaultContainer;
    row.primaryDnsIsAmnezia = (server.dns1 == protocols::dns::amneziaDnsIp);
    row.hasInstalledVpnContainers = computeHasInstalledVpnContainers(server.containers);
    return row;
}

QString getBaseDescription(bool primaryDnsIsAmnezia)
{
    return primaryDnsIsAmnezia ? QStringLiteral("Amnezia DNS | ") : QString();
}

QString getProtocolName(DockerContainer defaultContainer, const QMap<DockerContainer, ContainerConfig> &containers)
{
    QString containerName = ContainerUtils::containerHumanNames().value(defaultContainer);
    QString protocolVersion;

    if (ContainerUtils::isAwgContainer(defaultContainer)) {
        const auto it = containers.constFind(defaultContainer);
        if (it != containers.cend()) {
            if (const AwgProtocolConfig *awg = it->getAwgProtocolConfig()) {
                QString version = awg->clientProtocolVersion();
                if (version.isEmpty()) {
                    version = awg->serverProtocolVersion();
                }
                protocolVersion = AwgProtocolConfig::protocolVersionString(version);
                if (defaultContainer == DockerContainer::Awg && !awg->serverConfig.isThirdPartyConfig) {
                    containerName = QStringLiteral("AmneziaWG Legacy");
                }
            }
        }
    }

    return containerName + protocolVersion + QStringLiteral(" | ");
}

template <typename T>
ServerDescription buildDescription(const T &server)
{
    ServerDescription row = buildBaseDescription(server);
    row.serverName = server.displayName;
    row.baseDescription = getBaseDescription(row.primaryDnsIsAmnezia);

    const QString protocolName = getProtocolName(server.defaultContainer, server.containers);
    row.expandedServerDescription = row.baseDescription + row.hostName;
    row.collapsedServerDescription = row.baseDescription + protocolName + row.hostName;
    return row;
}

} // namespace

namespace amnezia
{

ServerDescription buildServerDescription(const SelfHostedUserServerConfig &server)
{
    return buildDescription(server);
}

ServerDescription buildServerDescription(const NativeServerConfig &server)
{
    return buildDescription(server);
}

} // namespace amnezia
