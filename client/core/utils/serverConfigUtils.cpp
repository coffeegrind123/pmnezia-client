#include "serverConfigUtils.h"

#include <QJsonArray>
#include <QJsonValue>

#include "core/models/selfhosted/selfHostedAdminServerConfig.h"
#include "core/utils/constants/configKeys.h"

namespace
{

bool hasThirdPartyConfig(const QJsonObject &json)
{
    const QJsonArray containersArray = json.value(amnezia::configKey::containers).toArray();
    for (const QJsonValue &val : containersArray) {
        const QJsonObject containerObj = val.toObject();
        for (auto it = containerObj.begin(); it != containerObj.end(); ++it) {
            if (it.key() == amnezia::configKey::container) {
                continue;
            }
            const QJsonObject protocolObj = it.value().toObject();
            if (protocolObj.contains(amnezia::configKey::isThirdPartyConfig)
                && protocolObj.value(amnezia::configKey::isThirdPartyConfig).toBool()) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

namespace serverConfigUtils
{

ConfigType configTypeFromJson(const QJsonObject &serverConfigObject)
{
    if (hasThirdPartyConfig(serverConfigObject)) {
        return ConfigType::Native;
    }

    const amnezia::SelfHostedAdminServerConfig adminProbe =
            amnezia::SelfHostedAdminServerConfig::fromJson(serverConfigObject);
    return adminProbe.hasCredentials() ? ConfigType::SelfHostedAdmin : ConfigType::SelfHostedUser;
}

} // namespace serverConfigUtils
