// SPDX-License-Identifier: GPL-3.0-or-later

#include "trustTunnelProtocolConfig.h"

#include <QJsonDocument>

#include "core/utils/constants/configKeys.h"
#include "core/utils/constants/protocolConstants.h"

namespace amnezia
{

namespace
{

// "host:port", "[v6]:port" or a bare host; the port of the first address.
QString portOf(const QString &address)
{
    const int colon = address.lastIndexOf(':');
    const bool bareIpv6 = !address.startsWith('[') && address.count(':') > 1;
    if (colon < 0 || bareIpv6 || (address.startsWith('[') && !address.left(colon).endsWith(']'))) {
        return QString::fromLatin1(protocols::trustTunnel::defaultPort);
    }
    return address.mid(colon + 1);
}

} // namespace

QJsonObject TrustTunnelServerConfig::toJson() const
{
    QJsonObject obj;
    if (!port.isEmpty()) {
        obj[configKey::port] = port;
    }
    if (!transportProto.isEmpty()) {
        obj[configKey::transportProto] = transportProto;
    }
    if (isThirdPartyConfig) {
        obj[configKey::isThirdPartyConfig] = true;
    }
    return obj;
}

TrustTunnelServerConfig TrustTunnelServerConfig::fromJson(const QJsonObject &json)
{
    TrustTunnelServerConfig c;
    c.port = json.value(configKey::port).toString();
    c.transportProto = json.value(configKey::transportProto).toString();
    c.isThirdPartyConfig = json.value(configKey::isThirdPartyConfig).toBool(false);
    return c;
}

QJsonObject TrustTunnelClientConfig::toJson() const
{
    QJsonObject obj = endpoint.toJson();
    if (!id.isEmpty()) {
        obj[configKey::clientId] = id;
    }
    return obj;
}

TrustTunnelClientConfig TrustTunnelClientConfig::fromJson(const QJsonObject &json)
{
    TrustTunnelClientConfig c;
    c.endpoint = serialization::trusttunnel::Endpoint::fromJson(json);
    c.id = json.value(configKey::clientId).toString();
    return c;
}

QJsonObject TrustTunnelProtocolConfig::toJson() const
{
    QJsonObject obj = serverConfig.toJson();
    if (clientConfig.has_value()) {
        obj[configKey::lastConfig] =
                QString::fromUtf8(QJsonDocument(clientConfig->toJson()).toJson(QJsonDocument::Compact));
    }
    return obj;
}

TrustTunnelProtocolConfig TrustTunnelProtocolConfig::fromJson(const QJsonObject &json)
{
    TrustTunnelProtocolConfig c;
    c.serverConfig = TrustTunnelServerConfig::fromJson(json);
    const QString last = json.value(configKey::lastConfig).toString();
    if (!last.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(last.toUtf8());
        if (doc.isObject()) {
            c.clientConfig = TrustTunnelClientConfig::fromJson(doc.object());
        }
    }
    return c;
}

TrustTunnelProtocolConfig TrustTunnelProtocolConfig::fromEndpoint(const serialization::trusttunnel::Endpoint &endpoint)
{
    TrustTunnelProtocolConfig c;
    c.serverConfig.port = endpoint.addresses.isEmpty()
            ? QString::fromLatin1(protocols::trustTunnel::defaultPort)
            : portOf(endpoint.addresses.first());
    c.serverConfig.transportProto =
            endpoint.upstreamProtocol == QLatin1String(protocols::trustTunnel::upstreamHttp3) ? QStringLiteral("udp")
                                                                                             : QStringLiteral("tcp");
    c.serverConfig.isThirdPartyConfig = true;

    TrustTunnelClientConfig client;
    client.endpoint = endpoint;
    client.id = endpoint.username;
    c.clientConfig = client;
    return c;
}

bool TrustTunnelProtocolConfig::hasClientConfig() const
{
    return clientConfig.has_value();
}

void TrustTunnelProtocolConfig::setClientConfig(const TrustTunnelClientConfig &config)
{
    clientConfig = config;
}

void TrustTunnelProtocolConfig::clearClientConfig()
{
    clientConfig.reset();
}

} // namespace amnezia
