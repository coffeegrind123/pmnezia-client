#include "serverConfigUtils.h"

#include <QJsonArray>
#include <QJsonValue>

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

int configFormatVersion(const QJsonObject &serverConfigObject)
{
    return serverConfigObject.value(amnezia::configKey::formatVersion).toInt(0);
}

bool isConfigFormatVersionSupported(const QJsonObject &serverConfigObject)
{
    return configFormatVersion(serverConfigObject) <= currentConfigFormatVersion;
}

ConfigType configTypeFromJson(const QJsonObject &serverConfigObject)
{
    return hasThirdPartyConfig(serverConfigObject) ? ConfigType::Native : ConfigType::SelfHostedUser;
}

} // namespace serverConfigUtils
