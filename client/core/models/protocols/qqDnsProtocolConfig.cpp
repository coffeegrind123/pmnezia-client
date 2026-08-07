// SPDX-License-Identifier: GPL-3.0-or-later

#include "qqDnsProtocolConfig.h"

#include "core/utils/constants/configKeys.h"
#include "core/utils/constants/protocolConstants.h"

namespace amnezia
{

QJsonObject QqDnsProtocolConfig::toJson() const
{
    QJsonObject o;
    o[configKey::qqDnsIps] = dnsIps;
    o[configKey::qqSendDomains] = sendDomains;
    o[configKey::qqRecvDomains] = recvDomains;
    o[configKey::qqSendInterfaceIp] = sendInterfaceIp;
    o[configKey::qqReceiveInterfaceIp] = receiveInterfaceIp;
    o[configKey::qqReceivePort] = receivePort;
    o[configKey::qqMaxDomainLen] = maxDomainLen;
    o[configKey::qqMaxSubLen] = maxSubLen;
    o[configKey::qqRetries] = retries;
    o[configKey::qqSendQueryType] = sendQueryType;
    o[configKey::qqPacketsSendIntervalMs] = packetsSendIntervalMs;
    o[configKey::qqPacketsWaitTimeLimitMs] = packetsWaitTimeLimitMs;
    o[configKey::qqSendSockNumbers] = sendSockNumbers;
    o[configKey::qqAwg] = awg;
    if (!additionalConfig.isEmpty()) {
        o[configKey::qqAdditionalConfig] = additionalConfig;
    }
    if (!id.isEmpty()) {
        o[QStringLiteral("id")] = id;
    }
    return o;
}

QqDnsProtocolConfig QqDnsProtocolConfig::fromJson(const QJsonObject &json)
{
    using namespace protocols::qqDns;
    QqDnsProtocolConfig c;
    c.dnsIps = json.value(configKey::qqDnsIps).toArray();
    c.sendDomains = json.value(configKey::qqSendDomains).toArray();
    c.recvDomains = json.value(configKey::qqRecvDomains).toArray();
    c.sendInterfaceIp = json.value(configKey::qqSendInterfaceIp).toString();
    c.receiveInterfaceIp = json.value(configKey::qqReceiveInterfaceIp).toString();
    c.receivePort = json.value(configKey::qqReceivePort).toInt(QString(defaultPort).toInt());
    c.maxDomainLen = json.value(configKey::qqMaxDomainLen).toInt(defaultMaxDomainLen);
    c.maxSubLen = json.value(configKey::qqMaxSubLen).toInt(defaultMaxSubLen);
    c.retries = json.value(configKey::qqRetries).toInt(defaultRetries);
    c.sendQueryType = json.value(configKey::qqSendQueryType).toInt(defaultSendQueryType);
    c.packetsSendIntervalMs =
            json.value(configKey::qqPacketsSendIntervalMs).toInt(defaultPacketsSendIntervalMs);
    c.packetsWaitTimeLimitMs =
            json.value(configKey::qqPacketsWaitTimeLimitMs).toInt(defaultPacketsWaitTimeLimitMs);
    c.sendSockNumbers = json.value(configKey::qqSendSockNumbers).toInt(defaultSendSockNumbers);
    c.awg = json.value(configKey::qqAwg).toObject();
    c.additionalConfig = json.value(configKey::qqAdditionalConfig).toObject();
    c.id = json.value(QStringLiteral("id")).toString();
    return c;
}

} // namespace amnezia
