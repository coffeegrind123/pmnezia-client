#ifndef SERVERCONFIGUTILS_H
#define SERVERCONFIGUTILS_H

#include <QJsonObject>

namespace serverConfigUtils
{

enum ConfigType {
    SelfHostedUser = 9,
    Native,
    Invalid
};

ConfigType configTypeFromJson(const QJsonObject &serverConfigObject);

} // namespace serverConfigUtils

#endif // SERVERCONFIGUTILS_H
