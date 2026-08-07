// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef QQDNSPROTOCOLCONFIG_H
#define QQDNSPROTOCOLCONFIG_H

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace amnezia
{

// Config for the QQ-DNS (UDP-over-DNS) transport. Unlike MasterDnsVPN this is
// not a self-contained tunnel: the in-process engine binds a loopback UDP port
// and AmneziaWG runs on top of it. So the config carries BOTH the QQ-DNS
// transport parameters AND an embedded AmneziaWG config (`awg`) whose endpoint
// QqDnsProtocol rewrites to 127.0.0.1:<enginePort>.
//
// Field names in the serialised form match the native engine's snake_case blob
// (see protocolConstants::qqDns), so QqDnsProtocol can hand the object straight
// to the engine after stripping `awg`/`additionalConfig` — no translation.
struct QqDnsProtocolConfig
{
    // Public DNS resolvers the engine sends queries through. JSON array of
    // strings, each "ip[:port]" or "[v6]:port".
    QJsonArray dnsIps;

    // NS-delegated domains: send_domains point at the server's delegation,
    // recv_domains are this client's own delegation. JSON arrays of FQDNs.
    QJsonArray sendDomains;
    QJsonArray recvDomains;

    // Bind addresses ("" => 0.0.0.0). receivePort is the authoritative :53
    // listener; the engine's loopback app port (h_in) is auto-assigned.
    QString sendInterfaceIp;
    QString receiveInterfaceIp;
    int receivePort = 53;

    // Wire-shape parameters — must match the server.
    int maxDomainLen = 253;
    int maxSubLen = 63;
    int retries = 1;
    int sendQueryType = 1; // 1=A, 28=AAAA, 16=TXT
    int packetsSendIntervalMs = 1;
    int packetsWaitTimeLimitMs = 1000;
    int sendSockNumbers = 16;

    // The embedded AmneziaWG config (a full protocol config object) that runs
    // on top of the DNS tunnel. Its endpoint is rewritten to loopback at start.
    QJsonObject awg;

    // Free-form pass-through, carried for round-trip integrity.
    QJsonObject additionalConfig;

    // Stable identity + third-party-import marker (mirrors the other configs).
    QString id;
    bool isThirdPartyConfig = false;

    QJsonObject toJson() const;
    static QqDnsProtocolConfig fromJson(const QJsonObject &json);
};

} // namespace amnezia

#endif // QQDNSPROTOCOLCONFIG_H
