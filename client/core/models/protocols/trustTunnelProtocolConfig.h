// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TRUSTTUNNELPROTOCOLCONFIG_H
#define TRUSTTUNNELPROTOCOLCONFIG_H

#include <QJsonObject>
#include <QString>
#include <optional>

#include "core/utils/serialization/trustTunnel.h"

namespace amnezia
{

// What the server card shows. Everything needed to connect lives in the
// client half; this is derived from it at import.
struct TrustTunnelServerConfig
{
    QString port;
    // "tcp" for HTTP/2, "udp" for HTTP/3.
    QString transportProto;
    bool isThirdPartyConfig = false;

    QJsonObject toJson() const;
    static TrustTunnelServerConfig fromJson(const QJsonObject &json);
};

// One user's endpoint profile, stored in the [endpoint] TOML key spelling
// (see serialization::trusttunnel::Endpoint) under last_config.
struct TrustTunnelClientConfig
{
    serialization::trusttunnel::Endpoint endpoint;
    // Stable identity for the per-client UX; the endpoint username.
    QString id;

    QJsonObject toJson() const;
    static TrustTunnelClientConfig fromJson(const QJsonObject &json);
};

struct TrustTunnelProtocolConfig
{
    TrustTunnelServerConfig serverConfig;
    std::optional<TrustTunnelClientConfig> clientConfig;

    QJsonObject toJson() const;
    static TrustTunnelProtocolConfig fromJson(const QJsonObject &json);

    // Builds both halves from an imported endpoint profile.
    static TrustTunnelProtocolConfig fromEndpoint(const serialization::trusttunnel::Endpoint &endpoint);

    bool hasClientConfig() const;
    void setClientConfig(const TrustTunnelClientConfig &config);
    void clearClientConfig();
};

} // namespace amnezia

#endif // TRUSTTUNNELPROTOCOLCONFIG_H
