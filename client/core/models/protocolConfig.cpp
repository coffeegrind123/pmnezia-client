#include "protocolConfig.h"

#include "core/utils/protocolEnum.h"
#include "core/protocols/protocolUtils.h"
#include "core/utils/constants/configKeys.h"
#include "core/utils/constants/protocolConstants.h"
#include "core/utils/containerEnum.h"
#include "core/utils/containers/containerUtils.h"
#include "core/utils/protocolEnum.h"

namespace amnezia
{

using namespace ProtocolEnumNS;
using namespace ProtocolUtils;

Proto ProtocolConfig::type() const
{
    return std::visit([](auto&& arg) -> Proto {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, AwgProtocolConfig>) {
            return Proto::Awg;
        } else if constexpr (std::is_same_v<T, WireGuardProtocolConfig>) {
            return Proto::WireGuard;
        } else if constexpr (std::is_same_v<T, XrayProtocolConfig>) {
            return Proto::Xray;
        } else if constexpr (std::is_same_v<T, MasterDnsVpnProtocolConfig>) {
            return Proto::MasterDnsVpn;
        } else if constexpr (std::is_same_v<T, QqDnsProtocolConfig>) {
            return Proto::QqDns;
        }
        return Proto::Unknown;
    }, data);
}

QString ProtocolConfig::port() const
{
    return std::visit([](auto&& arg) -> QString {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, AwgProtocolConfig>) {
            return arg.serverConfig.port;
        } else if constexpr (std::is_same_v<T, WireGuardProtocolConfig>) {
            return arg.serverConfig.port;
        } else if constexpr (std::is_same_v<T, XrayProtocolConfig>) {
            return arg.serverConfig.port;
        } else if constexpr (std::is_same_v<T, MasterDnsVpnProtocolConfig>) {
            return arg.serverConfig.port;
        } else if constexpr (std::is_same_v<T, QqDnsProtocolConfig>) {
            return QString::number(arg.receivePort);
        }
        return QString();
    }, data);
}

QString ProtocolConfig::transportProto() const
{
    return std::visit([](auto&& arg) -> QString {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, AwgProtocolConfig>) {
            return arg.serverConfig.transportProto;
        } else if constexpr (std::is_same_v<T, WireGuardProtocolConfig>) {
            return arg.serverConfig.transportProto;
        } else if constexpr (std::is_same_v<T, XrayProtocolConfig>) {
            return arg.serverConfig.transportProto;
        } else if constexpr (std::is_same_v<T, MasterDnsVpnProtocolConfig>) {
            // Always UDP (DNS envelopes); the server-side TOML doesn't carry a
            // string in the same wire slot, so we synthesize the literal here.
            return QStringLiteral("udp");
        } else if constexpr (std::is_same_v<T, QqDnsProtocolConfig>) {
            return QStringLiteral("udp");
        }
        return QString();
    }, data);
}

bool ProtocolConfig::hasClientConfig() const
{
    return std::visit([](auto&& arg) -> bool {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, AwgProtocolConfig> ||
                      std::is_same_v<T, WireGuardProtocolConfig> ||
                      std::is_same_v<T, XrayProtocolConfig> ||
                      std::is_same_v<T, MasterDnsVpnProtocolConfig> ||
                      std::is_same_v<T, QqDnsProtocolConfig>) {
            return arg.hasClientConfig();
        }
        return false;
    }, data);
}

QString ProtocolConfig::clientId() const
{
    return std::visit([](auto&& arg) -> QString {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, AwgProtocolConfig>) {
            if (arg.clientConfig.has_value()) {
                return arg.clientConfig->clientId;
            }
        } else if constexpr (std::is_same_v<T, WireGuardProtocolConfig>) {
            if (arg.clientConfig.has_value()) {
                return arg.clientConfig->clientId;
            }
        } else if constexpr (std::is_same_v<T, XrayProtocolConfig>) {
            if (arg.clientConfig.has_value()) {
                return arg.clientConfig->id;
            }
        } else if constexpr (std::is_same_v<T, MasterDnsVpnProtocolConfig>) {
            if (arg.clientConfig.has_value()) {
                return arg.clientConfig->id;
            }
        } else if constexpr (std::is_same_v<T, QqDnsProtocolConfig>) {
            return arg.id;
        }
        return QString();
    }, data);
}

QJsonObject ProtocolConfig::getClientConfigJson() const
{
    return std::visit([](auto&& arg) -> QJsonObject {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, AwgProtocolConfig>) {
            if (arg.hasClientConfig()) {
                return arg.clientConfig->toJson();
            }
        } else if constexpr (std::is_same_v<T, WireGuardProtocolConfig>) {
            if (arg.hasClientConfig()) {
                return arg.clientConfig->toJson();
            }
        } else if constexpr (std::is_same_v<T, XrayProtocolConfig>) {
            if (arg.hasClientConfig()) {
                return arg.clientConfig->toJson();
            }
        } else if constexpr (std::is_same_v<T, MasterDnsVpnProtocolConfig>) {
            if (arg.hasClientConfig()) {
                return arg.clientConfig->toJson();
            }
        } else if constexpr (std::is_same_v<T, QqDnsProtocolConfig>) {
            // QQ-DNS has no server/client split: the whole flat object is the
            // engine blob QqDnsProtocol consumes (transport fields + "awg").
            if (arg.hasClientConfig()) {
                return arg.toJson();
            }
        }
        return QJsonObject();
    }, data);
}

void ProtocolConfig::setClientConfigJson(const QJsonObject& json)
{
    std::visit([&json](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, AwgProtocolConfig>) {
            arg.setClientConfig(AwgClientConfig::fromJson(json));
        } else if constexpr (std::is_same_v<T, WireGuardProtocolConfig>) {
            arg.setClientConfig(WireGuardClientConfig::fromJson(json));
        } else if constexpr (std::is_same_v<T, XrayProtocolConfig>) {
            arg.setClientConfig(XrayClientConfig::fromJson(json));
        } else if constexpr (std::is_same_v<T, MasterDnsVpnProtocolConfig>) {
            arg.setClientConfig(MasterDnsVpnClientConfig::fromJson(json));
        } else if constexpr (std::is_same_v<T, QqDnsProtocolConfig>) {
            const bool thirdParty = arg.isThirdPartyConfig;
            arg = QqDnsProtocolConfig::fromJson(json);
            arg.isThirdPartyConfig = arg.isThirdPartyConfig || thirdParty;
        }
    }, data);
}

void ProtocolConfig::clearClientConfig()
{
    std::visit([](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, AwgProtocolConfig> ||
                      std::is_same_v<T, WireGuardProtocolConfig> ||
                      std::is_same_v<T, XrayProtocolConfig> ||
                      std::is_same_v<T, MasterDnsVpnProtocolConfig> ||
                      std::is_same_v<T, QqDnsProtocolConfig>) {
            arg.clearClientConfig();
        }
    }, data);
}

QString ProtocolConfig::nativeConfig() const
{
    return std::visit([](auto&& arg) -> QString {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, AwgProtocolConfig>) {
            if (arg.clientConfig.has_value()) {
                return arg.clientConfig->nativeConfig;
            }
        } else if constexpr (std::is_same_v<T, WireGuardProtocolConfig>) {
            if (arg.clientConfig.has_value()) {
                return arg.clientConfig->nativeConfig;
            }
        } else if constexpr (std::is_same_v<T, XrayProtocolConfig>) {
            if (arg.clientConfig.has_value()) {
                return arg.clientConfig->nativeConfig;
            }
        } else if constexpr (std::is_same_v<T, MasterDnsVpnProtocolConfig>) {
            // No imported-config-as-string slot for MasterDnsVPN: per-client
            // state is structured JSON consumed by the in-process engine.
            // The "nativeConfig" concept doesn't apply.
            return QString();
        } else if constexpr (std::is_same_v<T, QqDnsProtocolConfig>) {
            // Same as MasterDnsVPN: structured engine JSON, no config string.
            return QString();
        }
        return QString();
    }, data);
}

void ProtocolConfig::setNativeConfig(const QString &config)
{
    std::visit([&config](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, AwgProtocolConfig>) {
            if (arg.clientConfig.has_value()) {
                arg.clientConfig->nativeConfig = config;
            }
        } else if constexpr (std::is_same_v<T, WireGuardProtocolConfig>) {
            if (arg.clientConfig.has_value()) {
                arg.clientConfig->nativeConfig = config;
            }
        } else if constexpr (std::is_same_v<T, XrayProtocolConfig>) {
            if (arg.clientConfig.has_value()) {
                arg.clientConfig->nativeConfig = config;
            }
        } else if constexpr (std::is_same_v<T, MasterDnsVpnProtocolConfig>) {
            // No-op: see ProtocolConfig::nativeConfig() above for rationale.
            (void)config;
        } else if constexpr (std::is_same_v<T, QqDnsProtocolConfig>) {
            (void)config;
        }
    }, data);
}

bool ProtocolConfig::isThirdPartyConfig() const
{
    return std::visit([](auto&& arg) -> bool {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, AwgProtocolConfig> ||
                      std::is_same_v<T, WireGuardProtocolConfig> ||
                      std::is_same_v<T, XrayProtocolConfig> ||
                      std::is_same_v<T, MasterDnsVpnProtocolConfig>) {
            return arg.serverConfig.isThirdPartyConfig;
        } else if constexpr (std::is_same_v<T, QqDnsProtocolConfig>) {
            return arg.isThirdPartyConfig;
        }
        return false;
    }, data);
}

QJsonObject ProtocolConfig::toJson() const
{
    return std::visit([](auto&& arg) -> QJsonObject {
        return arg.toJson();
    }, data);
}

ProtocolConfig ProtocolConfig::fromJson(const QJsonObject& json, Proto type)
{
    switch (type) {
    case Proto::Awg:
        return ProtocolConfig{AwgProtocolConfig::fromJson(json)};
    case Proto::WireGuard:
        return ProtocolConfig{WireGuardProtocolConfig::fromJson(json)};
    case Proto::Xray:
        return ProtocolConfig{XrayProtocolConfig::fromJson(json)};
    case Proto::MasterDnsVpn:
        return ProtocolConfig{MasterDnsVpnProtocolConfig::fromJson(json)};
    case Proto::QqDns:
        return ProtocolConfig{QqDnsProtocolConfig::fromJson(json)};
    default:
        return ProtocolConfig{AwgProtocolConfig{}};
    }
}

} // namespace amnezia
