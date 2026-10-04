#include "utils/testUtils.h"

#include <QByteArray>
#include <QDebug>

#include "core/repositories/secureServersRepository.h"
#include "core/utils/serverConfigUtils.h"

namespace amnezia::test
{

QString getEnvValue(const QString &key)
{
    const QByteArray keyUtf8 = key.toUtf8();
    if (qEnvironmentVariableIsSet(keyUtf8.constData())) {
        return qEnvironmentVariable(keyUtf8.constData());
    }

    return QString();
}

void logEnvValueState(const QString &key)
{
    const QString value = getEnvValue(key);
    qInfo().noquote() << QString("%1: configured=%2 length=%3 value=\"%4\"")
                             .arg(key,
                                  isEnvValueConfigured(value) ? "yes" : "no",
                                  QString::number(value.size()),
                                  value);
}

bool isEnvValueConfigured(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    if (trimmed.endsWith(QStringLiteral("_config_string"))) {
        return false;
    }
    return true;
}

std::optional<QString> serverDescription(SecureServersRepository *repo, const QString &serverId)
{
    switch (repo->serverKind(serverId)) {
    case serverConfigUtils::ConfigType::SelfHostedUser: {
        const auto cfg = repo->selfHostedUserConfig(serverId);
        return cfg ? std::optional<QString>(cfg->description) : std::nullopt;
    }
    case serverConfigUtils::ConfigType::Native: {
        const auto cfg = repo->nativeConfig(serverId);
        return cfg ? std::optional<QString>(cfg->description) : std::nullopt;
    }
    default:
        return std::nullopt;
    }
}

std::optional<QString> serverDescriptionAt(SecureServersRepository *repo, int index)
{
    return serverDescription(repo, repo->serverIdAt(index));
}

void setServerDescription(SecureServersRepository *repo, const QString &serverId, const QString &description)
{
    const auto kind = repo->serverKind(serverId);
    switch (kind) {
    case serverConfigUtils::ConfigType::SelfHostedUser: {
        auto cfg = repo->selfHostedUserConfig(serverId);
        if (!cfg) {
            return;
        }
        cfg->description = description;
        cfg->displayName = description;
        repo->editServer(serverId, cfg->toJson(), kind);
        return;
    }
    case serverConfigUtils::ConfigType::Native: {
        auto cfg = repo->nativeConfig(serverId);
        if (!cfg) {
            return;
        }
        cfg->description = description;
        cfg->displayName = description;
        repo->editServer(serverId, cfg->toJson(), kind);
        return;
    }
    default:
        return;
    }
}

} // namespace amnezia::test
