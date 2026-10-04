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

constexpr int currentConfigFormatVersion = 1;

int configFormatVersion(const QJsonObject &serverConfigObject);

bool isConfigFormatVersionSupported(const QJsonObject &serverConfigObject);

ConfigType configTypeFromJson(const QJsonObject &serverConfigObject);

} // namespace serverConfigUtils

#endif // SERVERCONFIGUTILS_H
