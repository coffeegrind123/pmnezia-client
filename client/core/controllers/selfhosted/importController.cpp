#include "importController.h"

#include <QDataStream>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMap>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>
#include <QUrl>
#include <algorithm>

#include "core/utils/containerEnum.h"
#include "core/utils/containers/containerUtils.h"
#include "core/utils/protocolEnum.h"
#include "core/models/protocols/masterDnsVpnProtocolConfig.h"
#include "core/models/protocols/qqDnsProtocolConfig.h"
#include "core/utils/serverConfigUtils.h"
#include "core/utils/serialization/serialization.h"
#include "core/utils/utilities.h"
#include "core/utils/protocolEnum.h"
#include "core/protocols/protocolUtils.h"
#include "core/utils/constants/configKeys.h"
#include "core/utils/constants/protocolConstants.h"
#include "core/utils/qrCodeUtils.h"

using namespace amnezia;
using namespace ProtocolUtils;

namespace
{
    ConfigTypes checkConfigFormat(const QString &config)
    {
        const QString openVpnConfigPatternCli = "client";
        const QString openVpnConfigPatternDriver1 = "dev tun";
        const QString openVpnConfigPatternDriver2 = "dev tap";

        const QString wireguardConfigPatternSectionInterface = "[Interface]";
        const QString wireguardConfigPatternSectionPeer = "[Peer]";

        const QString xrayConfigPatternInbound = "inbounds";
        const QString xrayConfigPatternOutbound = "outbounds";

        const QString amneziaConfigPattern = "containers";
        const QString amneziaConfigPatternHostName = "hostName";
        const QString amneziaConfigPatternUserName = "userName";
        const QString amneziaConfigPatternPassword = "password";
        const QString backupPattern = "Servers/serversList";

        // MasterDnsVPN client_config JSON (upstream UPPER_SNAKE schema). Two
        // distinctive keys avoid matching anything else; the Amnezia-envelope
        // check above already wins for a wrapped config that also carries them.
        const QString masterDnsVpnPatternDomains = "DOMAINS";
        const QString masterDnsVpnPatternMethod = "DATA_ENCRYPTION_METHOD";

        if (config.contains(backupPattern)) {
            return ConfigTypes::Backup;
        } else if (config.contains(amneziaConfigPattern)
                   || (config.contains(amneziaConfigPatternHostName) && config.contains(amneziaConfigPatternUserName)
                       && config.contains(amneziaConfigPatternPassword))) {
            return ConfigTypes::Amnezia;
        } else if (config.contains(wireguardConfigPatternSectionInterface) && config.contains(wireguardConfigPatternSectionPeer)) {
            return ConfigTypes::WireGuard;
        } else if (config.contains(masterDnsVpnPatternDomains) && config.contains(masterDnsVpnPatternMethod)) {
            return ConfigTypes::MasterDnsVpn;
        } else if (config.contains("send_domains") && config.contains("recv_domains")) {
            // QQ-DNS import JSON: a "qqdns" transport block + an embedded "awg"
            // config. The two snake_case domain keys are distinctive; the
            // Amnezia-envelope / WireGuard checks above already win for wrapped
            // configs that happen to carry them.
            return ConfigTypes::QqDns;
        } else if ((config.contains(xrayConfigPatternInbound)) && (config.contains(xrayConfigPatternOutbound))) {
            return ConfigTypes::Xray;
        } else if (config.contains(openVpnConfigPatternCli)
                   && (config.contains(openVpnConfigPatternDriver1) || config.contains(openVpnConfigPatternDriver2))) {
            return ConfigTypes::OpenVpn;
        }
        return ConfigTypes::Invalid;
    }
} // namespace

ImportController::ImportController(SecureServersRepository* serversRepository,
                                   SecureAppSettingsRepository* appSettingsRepository,
                                   QObject *parent)
    : QObject(parent),
      m_serversRepository(serversRepository),
      m_appSettingsRepository(appSettingsRepository)
{
}

ImportController::ImportResult ImportController::extractConfigFromData(const QString &data, const QString &configFileName)
{
    ImportResult result;
    result.configFileName = configFileName;
    result.maliciousWarningText.clear();

    QString config = data;
    QString prefix;
    QString errormsg;
    ConfigTypes configType = ConfigTypes::Invalid;

    if (config.startsWith("vless://")) {
        configType = ConfigTypes::Xray;
        result.config = extractXrayConfig(
                Utils::JsonToString(serialization::vless::Deserialize(config, &prefix, &errormsg), QJsonDocument::JsonFormat::Compact),
                configType, prefix);
        if (!result.config.empty()) {
            result.configType = configType;
            return result;
        }
    }

    if (config.startsWith("vmess://") && config.contains("@")) {
        configType = ConfigTypes::Xray;
        result.config = extractXrayConfig(
                Utils::JsonToString(serialization::vmess_new::Deserialize(config, &prefix, &errormsg), QJsonDocument::JsonFormat::Compact),
                configType, prefix);
        if (!result.config.empty()) {
            result.configType = configType;
            return result;
        }
    }

    if (config.startsWith("vmess://")) {
        configType = ConfigTypes::Xray;
        result.config = extractXrayConfig(
                Utils::JsonToString(serialization::vmess::Deserialize(config, &prefix, &errormsg), QJsonDocument::JsonFormat::Compact),
                configType, prefix);
        if (!result.config.empty()) {
            result.configType = configType;
            return result;
        }
    }

    if (config.startsWith("trojan://")) {
        configType = ConfigTypes::Xray;
        result.config = extractXrayConfig(
                Utils::JsonToString(serialization::trojan::Deserialize(config, &prefix, &errormsg), QJsonDocument::JsonFormat::Compact),
                configType, prefix);
        if (!result.config.empty()) {
            result.configType = configType;
            return result;
        }
    }

    if (config.startsWith("ss://") && !config.contains("plugin=")) {
        configType = ConfigTypes::ShadowSocks;
        result.config = extractXrayConfig(
                Utils::JsonToString(serialization::ss::Deserialize(config, &prefix, &errormsg), QJsonDocument::JsonFormat::Compact),
                configType, prefix);
        if (!result.config.empty()) {
            result.configType = configType;
            return result;
        }
    }

    if (config.startsWith("ssd://")) {
        QStringList tmp;
        QList<std::pair<QString, QJsonObject>> servers = serialization::ssd::Deserialize(config, &prefix, &tmp);
        configType = ConfigTypes::ShadowSocks;
        // Took only first config from list
        if (!servers.isEmpty()) {
            result.config = extractXrayConfig(servers.first().first, configType);
        }
        if (!result.config.empty()) {
            result.configType = configType;
            return result;
        }
    }

    if (config.startsWith("mdnsvpn://")) {
        configType = ConfigTypes::MasterDnsVpn;
        // Share blob shape: mdnsvpn://b64?<base64-json>[&resolvers=<csv>].
        // Tolerate a bare mdnsvpn://<base64-json> too. The base64 is standard
        // (alphabet is A-Za-z0-9+/=), so neither '?' nor '&' can occur inside
        // it and both delimiters are unambiguous.
        QString rest = config.mid(QStringLiteral("mdnsvpn://").size());
        const int q = rest.indexOf('?');
        QString payload = (q >= 0) ? rest.mid(q + 1) : rest;

        // The trailing `&resolvers=` list is an awg-easy-rs extension carrying
        // the resolver set that used to ride inside the JSON body. It must be
        // split off before decoding: QByteArray::fromBase64 is non-strict and
        // silently absorbs the alphabet-legal characters of "resolvers" into
        // the payload, which corrupts the decode whenever the base64 carries
        // no '=' padding to stop it early.
        QStringList urlResolvers;
        const int amp = payload.indexOf('&');
        if (amp >= 0) {
            const QString query = payload.mid(amp + 1);
            payload.truncate(amp);
            for (const QString &param : query.split('&', Qt::SkipEmptyParts)) {
                if (!param.startsWith(QStringLiteral("resolvers="))) {
                    continue;
                }
                const QString list = QUrl::fromPercentEncoding(
                        param.mid(QStringLiteral("resolvers=").size()).toUtf8());
                for (const QString &r : list.split(',', Qt::SkipEmptyParts)) {
                    const QString trimmed = r.trimmed();
                    if (!trimmed.isEmpty()) {
                        urlResolvers.append(trimmed);
                    }
                }
            }
        }

        const QByteArray decoded = QByteArray::fromBase64(payload.toUtf8());
        result.config = extractMasterDnsVpnConfig(QString::fromUtf8(decoded), "", urlResolvers);
        if (!result.config.empty()) {
            result.configType = configType;
            return result;
        }
        result.errorCode = ErrorCode::ImportInvalidConfigError;
        return result;
    }

    configType = checkConfigFormat(config);
    if (configType == ConfigTypes::Invalid) {
        config.replace("vpn://", "");
        QByteArray ba = QByteArray::fromBase64(config.toUtf8(), QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        QByteArray baUncompressed = qUncompress(ba);
        if (!baUncompressed.isEmpty()) {
            ba = baUncompressed;
        }

        config = ba;
        configType = checkConfigFormat(config);
    }

    result.configType = configType;

    switch (configType) {
    case ConfigTypes::OpenVpn: {
        result.config = extractOpenVpnConfig(config);
        if (!result.config.empty()) {
            checkForMaliciousStrings(result.config, result.maliciousWarningText);
            return result;
        }
        result.errorCode = ErrorCode::ImportInvalidConfigError;
        return result;
    }
    case ConfigTypes::Awg:
    case ConfigTypes::WireGuard: {
        result.config = extractWireGuardConfig(config, result.configType);
        result.isNativeWireGuardConfig = (result.configType == ConfigTypes::WireGuard);
        if (!result.config.empty()) {
            return result;
        }
        result.errorCode = ErrorCode::ImportInvalidConfigError;
        return result;
    }
    case ConfigTypes::Xray: {
        result.config = extractXrayConfig(config, configType);
        if (!result.config.empty()) {
            return result;
        }
        result.errorCode = ErrorCode::ImportInvalidConfigError;
        return result;
    }
    case ConfigTypes::MasterDnsVpn: {
        result.config = extractMasterDnsVpnConfig(config);
        if (!result.config.empty()) {
            return result;
        }
        result.errorCode = ErrorCode::ImportInvalidConfigError;
        return result;
    }
    case ConfigTypes::QqDns: {
        result.config = extractQqDnsConfig(config);
        if (!result.config.empty()) {
            return result;
        }
        result.errorCode = ErrorCode::ImportInvalidConfigError;
        return result;
    }
    case ConfigTypes::Amnezia: {
        result.config = QJsonDocument::fromJson(config.toUtf8()).object();

        processAmneziaConfig(result.config);
        if (!result.config.empty()) {
            checkForMaliciousStrings(result.config, result.maliciousWarningText);
            return result;
        }
        result.errorCode = ErrorCode::ImportInvalidConfigError;
        return result;
    }
    case ConfigTypes::Backup: {
        result.errorCode = ErrorCode::ImportBackupFileUseRestoreInstead;
        return result;
    }
    case ConfigTypes::Invalid: {
        result.errorCode = ErrorCode::ImportInvalidConfigError;
        result.configFileName.clear();
        return result;
    }
    }
    
    result.errorCode = ErrorCode::ImportInvalidConfigError;
    return result;
}

ImportController::ImportResult ImportController::extractConfigFromQr(const QByteArray &data)
{
    ImportResult result;

    QString dataStr = QString::fromUtf8(data);
    // Scheme-prefixed share strings (e.g. the MasterDnsVPN `mdnsvpn://b64?`
    // blob) aren't content-sniffable, so route them straight through the
    // string importer which handles the scheme.
    if (dataStr.startsWith("mdnsvpn://")) {
        return extractConfigFromData(dataStr, "");
    }
    ConfigTypes configType = checkConfigFormat(dataStr);
    if (configType != ConfigTypes::Invalid) {
        return extractConfigFromData(dataStr, "");
    }

    QJsonObject dataObj = QJsonDocument::fromJson(data).object();
    if (!dataObj.isEmpty()) {
        result.config = dataObj;
        result.configType = ConfigTypes::Amnezia;
        return result;
    }

    QByteArray ba_uncompressed = qUncompress(data);
    if (!ba_uncompressed.isEmpty()) {
        result.config = QJsonDocument::fromJson(ba_uncompressed).object();
        if (result.config.isEmpty()) {
            result.errorCode = ErrorCode::ImportInvalidConfigError;
            return result;
        }
        result.configType = ConfigTypes::Amnezia;
        return result;
    }

    QByteArray ba = QByteArray::fromBase64(data, QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    QByteArray baUncompressed = qUncompress(ba);

    if (!baUncompressed.isEmpty()) {
        ba = baUncompressed;
    }

    if (!ba.isEmpty()) {
        result.config = QJsonDocument::fromJson(ba).object();
        if (result.config.isEmpty()) {
            result.errorCode = ErrorCode::ImportInvalidConfigError;
            return result;
        }
        result.configType = ConfigTypes::Amnezia;
        return result;
    }

    result.errorCode = ErrorCode::ImportInvalidConfigError;
    return result;
}

void ImportController::startDecodingQr()
{
    m_qrCodeChunks.clear();
    m_totalQrCodeChunksCount = 0;
    m_receivedQrCodeChunksCount = 0;
    m_isQrCodeProcessed = true;
}

ImportController::QrParseResult ImportController::parseQrCodeChunk(const QString &code)
{
    QrParseResult parseResult;
    parseResult.chunksReceived = m_receivedQrCodeChunksCount;
    parseResult.chunksTotal = m_totalQrCodeChunksCount;

    if (!m_isQrCodeProcessed) {
        return parseResult;
    }

    QByteArray ba = QByteArray::fromBase64(code.toUtf8(), QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    QDataStream s(&ba, QIODevice::ReadOnly);
    qint16 magic;
    s >> magic;

    if (magic == qrCodeUtils::qrMagicCode) {
        quint8 chunksCount;
        s >> chunksCount;
        if (m_totalQrCodeChunksCount != chunksCount) {
            m_qrCodeChunks.clear();
        }

        m_totalQrCodeChunksCount = chunksCount;

        quint8 chunkId;
        s >> chunkId;
        s >> m_qrCodeChunks[chunkId];
        m_receivedQrCodeChunksCount = m_qrCodeChunks.size();
        parseResult.chunksReceived = m_receivedQrCodeChunksCount;
        parseResult.chunksTotal = m_totalQrCodeChunksCount;

        if (m_qrCodeChunks.size() == m_totalQrCodeChunksCount) {
            QByteArray data;
            for (int i = 0; i < m_totalQrCodeChunksCount; ++i) {
                data.append(m_qrCodeChunks.value(i));
            }

            ImportResult result = extractConfigFromQr(data);
            if (result.errorCode == ErrorCode::NoError) {
                parseResult.success = true;
                parseResult.importResult = result;
                m_isQrCodeProcessed = false;
            } else {
                m_qrCodeChunks.clear();
                m_totalQrCodeChunksCount = 0;
                m_receivedQrCodeChunksCount = 0;
            }
        }
    } else {
        ImportResult result = extractConfigFromQr(code.toUtf8());
        if (result.errorCode != ErrorCode::NoError) {
            result = extractConfigFromQr(ba);
        }
        if (result.errorCode == ErrorCode::NoError) {
            parseResult.success = true;
            parseResult.importResult = result;
            m_isQrCodeProcessed = false;
        }
    }

    return parseResult;
}

bool ImportController::isQrDecodingActive() const
{
    return m_isQrCodeProcessed;
}

int ImportController::qrChunksReceived() const
{
    return m_receivedQrCodeChunksCount;
}

int ImportController::qrChunksTotal() const
{
    return m_totalQrCodeChunksCount;
}

void ImportController::importConfig(const QJsonObject &config)
{
    // Every importable config carries its connection containers; there is no
    // credentials-only "empty server" to add any more.
    if (config.contains(configKey::containers)) {
        m_serversRepository->addServer(QString(), config, serverConfigUtils::configTypeFromJson(config));
        emit importFinished();
    } else {
        qDebug() << "Failed to import profile";
        qDebug().noquote() << QJsonDocument(config).toJson();
        emit importErrorOccurred(ErrorCode::ImportInvalidConfigError, false);
    }
}

QJsonObject ImportController::processNativeWireGuardConfig(const QJsonObject &config)
{
    QJsonObject result = config;
    auto containers = result.value(configKey::containers).toArray();
    if (!containers.isEmpty()) {
        auto container = containers.at(0).toObject();
        auto serverProtocolConfig = container.value(ContainerUtils::containerTypeToProtocolString(DockerContainer::WireGuard)).toObject();
        auto clientProtocolConfig = QJsonDocument::fromJson(serverProtocolConfig.value(configKey::lastConfig).toString().toUtf8()).object();

        QString junkPacketCount = QString::number(QRandomGenerator::global()->bounded(4, 7));
        QString junkPacketMinSize = QString::number(10);
        QString junkPacketMaxSize = QString::number(50);
        clientProtocolConfig[configKey::junkPacketCount] = junkPacketCount;
        clientProtocolConfig[configKey::junkPacketMinSize] = junkPacketMinSize;
        clientProtocolConfig[configKey::junkPacketMaxSize] = junkPacketMaxSize;
        clientProtocolConfig[configKey::initPacketJunkSize] = "0";
        clientProtocolConfig[configKey::responsePacketJunkSize] = "0";
        clientProtocolConfig[configKey::initPacketMagicHeader] = "1";
        clientProtocolConfig[configKey::responsePacketMagicHeader] = "2";
        clientProtocolConfig[configKey::underloadPacketMagicHeader] = "3";
        clientProtocolConfig[configKey::transportPacketMagicHeader] = "4";

        clientProtocolConfig[configKey::cookieReplyPacketJunkSize] = "0";
        clientProtocolConfig[configKey::transportPacketJunkSize] = "0";

        clientProtocolConfig[configKey::specialJunk1] = protocols::awg::defaultSpecialJunk1;

        clientProtocolConfig[configKey::isObfuscationEnabled] = true;

        serverProtocolConfig[configKey::lastConfig] = QString(QJsonDocument(clientProtocolConfig).toJson());
        container[configKey::wireguard] = serverProtocolConfig;
        containers.replace(0, container);
        result[configKey::containers] = containers;
    }
    return result;
}

ConfigTypes ImportController::checkConfigFormat(const QString &config) const
{
    return ::checkConfigFormat(config);
}

QJsonObject ImportController::extractOpenVpnConfig(const QString &data) const
{
    QJsonObject openVpnConfig;
    openVpnConfig[configKey::config] = data;

    QJsonObject lastConfig;
    lastConfig[configKey::lastConfig] = QString(QJsonDocument(openVpnConfig).toJson());
    lastConfig[configKey::isThirdPartyConfig] = true;

    QJsonObject containers;
    containers.insert(configKey::container, QJsonValue(configKey::amneziaOpenvpn));
    containers.insert(configKey::openvpn, QJsonValue(lastConfig));

    QJsonArray arr;
    arr.push_back(containers);

    QString hostName;
    const static QRegularExpression hostNameRegExp("remote\\s+([^\\s]+)");
    QRegularExpressionMatch hostNameMatch = hostNameRegExp.match(data);
    if (hostNameMatch.hasMatch()) {
        hostName = hostNameMatch.captured(1);
    }

    QJsonObject config;
    config[configKey::containers] = arr;
    config[configKey::defaultContainer] = configKey::amneziaOpenvpn;
    config[configKey::description] = m_serversRepository->nextAvailableServerName();

    const static QRegularExpression dnsRegExp("dhcp-option DNS (\\b\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\b)");
    QRegularExpressionMatchIterator dnsMatch = dnsRegExp.globalMatch(data);
    if (dnsMatch.hasNext()) {
        config[configKey::dns1] = dnsMatch.next().captured(1);
    }
    if (dnsMatch.hasNext()) {
        config[configKey::dns2] = dnsMatch.next().captured(1);
    }

    config[configKey::hostName] = hostName;

    return config;
}

QJsonObject ImportController::extractWireGuardConfig(const QString &data, ConfigTypes &configType) const
{
    QMap<QString, QString> configMap;
    auto configByLines = data.split("\n");
    for (const QString &line : configByLines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.startsWith("[") && trimmedLine.endsWith("]")) {
            continue;
        } else {
            const qsizetype separatorIndex = trimmedLine.indexOf('=');
            if (separatorIndex > 0) {
                configMap[trimmedLine.left(separatorIndex).trimmed()] =
                        trimmedLine.mid(separatorIndex + 1).trimmed();
            }
        }
    }

    QJsonObject lastConfig;
    lastConfig[configKey::config] = data;

    auto url { QUrl::fromUserInput(configMap.value(protocols::wireguard::Endpoint)) };
    QString hostName;
    QString port;
    if (!url.host().isEmpty()) {
        hostName = url.host();
    } else {
        qDebug() << "Key parameter" << protocols::wireguard::Endpoint << "is missing or has an invalid format";
        return QJsonObject();
    }

    if (url.port() != -1) {
        port = QString::number(url.port());
    } else {
        port = protocols::wireguard::defaultPort;
    }

    lastConfig[configKey::hostName] = hostName;
    lastConfig[configKey::port] = port.toInt();

    if (!configMap.value(protocols::wireguard::PrivateKey).isEmpty()
            && !configMap.value(protocols::wireguard::Address).isEmpty()
            && !configMap.value(protocols::wireguard::PublicKey).isEmpty()) {
        lastConfig[configKey::clientPrivKey] = configMap.value(protocols::wireguard::PrivateKey);
        lastConfig[configKey::clientIp] = configMap.value(protocols::wireguard::Address);

        if (!configMap.value(protocols::wireguard::PresharedKey).isEmpty()) {
            lastConfig[configKey::pskKey] = configMap.value(protocols::wireguard::PresharedKey);
        } else if (!configMap.value(protocols::wireguard::PreSharedKey).isEmpty()) {
            lastConfig[configKey::pskKey] = configMap.value(protocols::wireguard::PreSharedKey);
        }

        lastConfig[configKey::serverPubKey] = configMap.value(protocols::wireguard::PublicKey);
    } else {
        qDebug() << "One of the key parameters is missing (PrivateKey, Address, PublicKey)";
        return QJsonObject();
    }

    if (!configMap.value(protocols::wireguard::MTU).isEmpty()) {
        lastConfig[configKey::mtu] = configMap.value(protocols::wireguard::MTU);
    }

    if (!configMap.value(protocols::wireguard::PersistentKeepalive).isEmpty()) {
        lastConfig[configKey::persistentKeepAlive] = configMap.value(protocols::wireguard::PersistentKeepalive);
    }

    const QStringList allowedIps = configMap.value(protocols::wireguard::AllowedIPs).split(
            QRegularExpression("\\s*,\\s*"), Qt::SkipEmptyParts);
    QJsonArray allowedIpsJsonArray = QJsonArray::fromStringList(allowedIps);

    lastConfig[configKey::allowedIps] = allowedIpsJsonArray;

    QString protocolName = configKey::wireguard;
    ConfigTypes detectedType = ConfigTypes::WireGuard;

    const QStringList awgProtocolKeys = configKey::awgProtocolKeys();

    bool hasAwgKeys = std::any_of(awgProtocolKeys.begin(), awgProtocolKeys.end(),
                                    [&configMap](const QString &field) { return !configMap.value(field).isEmpty(); });
    if (hasAwgKeys) {
        for (const QString &key : awgProtocolKeys) {
            if (!configMap.value(key).isEmpty()) {
                lastConfig[key] = configMap.value(key);
            }
        }

        protocolName = configKey::awg;
        detectedType = ConfigTypes::Awg;
    }

    if (!configMap.value(protocols::wireguard::MTU).isEmpty()) {
        lastConfig[configKey::mtu] = configMap.value(protocols::wireguard::MTU);
    } else {
        lastConfig[configKey::mtu] = (protocolName == configKey::awg) 
                                       ? protocols::awg::defaultMtu 
                                       : protocols::wireguard::defaultMtu;
    }

    QJsonObject wireguardConfig;
    wireguardConfig[configKey::lastConfig] = QString(QJsonDocument(lastConfig).toJson());
    wireguardConfig[configKey::isThirdPartyConfig] = true;
    wireguardConfig[configKey::port] = port;
    wireguardConfig[configKey::transportProto] = protocols::openvpn::defaultTransportProto;

    QJsonObject containers;
    QString containerName = (protocolName == configKey::awg) ? configKey::amneziaAwg : configKey::amneziaWireguard;
    containers.insert(configKey::container, QJsonValue(containerName));
    containers.insert(protocolName, QJsonValue(wireguardConfig));

    QJsonArray arr;
    arr.push_back(containers);

    QJsonObject config;
    config[configKey::containers] = arr;
    config[configKey::defaultContainer] = containerName;
    config[configKey::description] = m_serversRepository->nextAvailableServerName();

    const static QRegularExpression dnsRegExp(
            "DNS = "
            "(\\b\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\b).*(\\b\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\b)");
    QRegularExpressionMatch dnsMatch = dnsRegExp.match(data);
    if (dnsMatch.hasMatch()) {
        config[configKey::dns1] = dnsMatch.captured(1);
        config[configKey::dns2] = dnsMatch.captured(2);
    }

    config[configKey::hostName] = hostName;

    configType = detectedType;
    return config;
}

QJsonObject ImportController::extractXrayConfig(const QString &data, ConfigTypes configType, const QString &description) const
{
    QJsonParseError parserErr;
    QJsonDocument jsonConf = QJsonDocument::fromJson(data.toLocal8Bit(), &parserErr);
    if (parserErr.error != QJsonParseError::NoError || !jsonConf.isObject()) {
        qDebug() << "Xray config JSON parse failed:" << parserErr.errorString();
        return QJsonObject();
    }

    const QJsonObject parsedConfig = jsonConf.object();
    if (!parsedConfig.value(protocols::xray::inbounds).isArray()
            || !parsedConfig.value(protocols::xray::outbounds).isArray()) {
        qDebug() << "Xray config is missing inbounds or outbounds";
        return QJsonObject();
    }

    const QString serializedConfig = QString::fromUtf8(jsonConf.toJson());

    QJsonObject xrayVpnConfig;
    xrayVpnConfig[configKey::config] = serializedConfig;
    QJsonObject lastConfig;
    lastConfig[configKey::lastConfig] = serializedConfig;
    lastConfig[configKey::isThirdPartyConfig] = true;

    QJsonObject containers;
    if (configType == ConfigTypes::ShadowSocks) {
        containers.insert(configKey::ssxray, QJsonValue(lastConfig));
        containers.insert(configKey::container, QJsonValue(configKey::amneziaSsxray));
    } else {
        containers.insert(configKey::container, QJsonValue(configKey::amneziaXray));
        containers.insert(configKey::xray, QJsonValue(lastConfig));
    }

    QJsonArray arr;
    arr.push_back(containers);

    QString hostName;

    const static QRegularExpression hostNameRegExp("\"address\":\\s*\"([^\"]+)");
    QRegularExpressionMatch hostNameMatch = hostNameRegExp.match(data);
    if (hostNameMatch.hasMatch()) {
        hostName = hostNameMatch.captured(1);
    }

    QJsonObject config;
    config[configKey::containers] = arr;
    config[configKey::defaultContainer] = (configType == ConfigTypes::ShadowSocks)
            ? configKey::amneziaSsxray
            : configKey::amneziaXray;
    if (description.isEmpty()) {
        config[configKey::description] = m_serversRepository->nextAvailableServerName();
    } else {
        config[configKey::description] = description;
    }
    config[configKey::hostName] = hostName;

    return config;
}

QJsonObject ImportController::extractMasterDnsVpnConfig(const QString &data, const QString &description,
                                                        const QStringList &urlResolvers) const
{
    QJsonParseError parserErr;
    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &parserErr);
    if (parserErr.error != QJsonParseError::NoError || !doc.isObject()) {
        return {};
    }
    const QJsonObject src = doc.object();

    // Map the upstream MasterDnsVPN client_config schema (UPPER_SNAKE keys, as
    // emitted by awg-easy-rs's `mdnsvpn://b64?` share blob and the upstream
    // `mdnsvpn -json_base64` format) onto the native model. The model's
    // camelCase keys differ from this schema, so the translation is explicit.
    MasterDnsVpnProtocolConfig protoConfig;

    MasterDnsVpnServerConfig &server = protoConfig.serverConfig;
    server.domains = src.value("DOMAINS").toArray();
    server.encryptionMethod =
            src.value("DATA_ENCRYPTION_METHOD").toInt(protocols::masterDnsVpn::defaultEncryptionMethod);
    server.encryptionKey = src.value("ENCRYPTION_KEY").toString();
    server.protocolType = src.value("PROTOCOL_TYPE").toString();
    server.isThirdPartyConfig = true;

    // Essentials — without a tunnel domain and a key the engine can't dial.
    if (server.domains.isEmpty() || server.encryptionKey.isEmpty()) {
        return {};
    }

    MasterDnsVpnClientConfig client;
    // LISTEN_PORT may arrive typed as a number (JSON) or a string; the model
    // stores it as a string.
    const QJsonValue listenPort = src.value("LISTEN_PORT");
    client.listenPort =
            listenPort.isString() ? listenPort.toString() : QString::number(listenPort.toInt());
    client.socks5User = src.value("SOCKS5_USER").toString();
    client.socks5Pass = src.value("SOCKS5_PASS").toString();
    // Resolver list. A RESOLVERS member is accepted for configs produced by
    // older servers, but current awg-easy-rs deliberately omits it (upstream
    // ignores it), so the share URL's `&resolvers=` list is the live source.
    client.resolvers = src.value("RESOLVERS").toArray();
    if (client.resolvers.isEmpty() && !urlResolvers.isEmpty()) {
        QJsonArray resolvers;
        for (const QString &r : urlResolvers) {
            resolvers.push_back(r);
        }
        client.resolvers = resolvers;
    }
    client.balancingStrategy = src.value("RESOLVER_BALANCING_STRATEGY").toInt(5);
    client.packetDuplication = src.value("PACKET_DUPLICATION_COUNT").toInt(3);
    client.setupPacketDuplication = src.value("SETUP_PACKET_DUPLICATION_COUNT").toInt(4);
    // Preserve the verbatim source for round-trip fidelity and forward compat:
    // engine-tuning keys we don't model (AUTO_*, LOG_LEVEL, LISTEN_IP, ...)
    // ride along here and are ignored by the engine.
    client.additionalConfig = src;
    protoConfig.setClientConfig(client);

    // Wrap into the canonical Amnezia container shape — mirrors
    // ContainerConfig::toJson(): containers[].<protoName> = ProtocolConfig::toJson().
    QJsonObject container;
    container[configKey::container] = ContainerUtils::containerToString(DockerContainer::MasterDnsVpn);
    container[ProtocolUtils::protoToString(Proto::MasterDnsVpn)] = protoConfig.toJson();

    QJsonArray containers;
    containers.push_back(container);

    QJsonObject config;
    config[configKey::containers] = containers;
    config[configKey::defaultContainer] = ContainerUtils::containerToString(DockerContainer::MasterDnsVpn);
    config[configKey::description] =
            description.isEmpty() ? m_serversRepository->nextAvailableServerName() : description;
    // Surface the first tunnel domain as the server card's host label.
    config[configKey::hostName] = server.domains.first().toString();

    return config;
}

QJsonObject ImportController::extractQqDnsConfig(const QString &data, const QString &description) const
{
    QJsonParseError parserErr;
    const QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &parserErr);
    if (parserErr.error != QJsonParseError::NoError || !doc.isObject()) {
        return {};
    }
    const QJsonObject src = doc.object();

    // Accept either a nested { "qqdns": {…}, "awg": {…} } document or a flat
    // object that carries the transport fields directly plus "awg".
    const QJsonObject qq =
            src.contains(configKey::qqAwg) || src.contains(QStringLiteral("qqdns"))
            ? src.value(QStringLiteral("qqdns")).toObject()
            : src;
    const QJsonObject transport = qq.isEmpty() ? src : qq;

    QqDnsProtocolConfig proto = QqDnsProtocolConfig::fromJson(transport);
    if (proto.sendDomains.isEmpty() || proto.recvDomains.isEmpty() || proto.dnsIps.isEmpty()) {
        return {};
    }

    // Normalise the embedded AmneziaWG config: the protocol expects
    // awg["awg_config_data"], so wrap a bare awg_config_data object if needed.
    QJsonObject awgSrc = src.value(configKey::qqAwg).toObject();
    if (awgSrc.isEmpty()) {
        awgSrc = qq.value(configKey::qqAwg).toObject();
    }
    QJsonObject awgWrapper;
    const QString awgDataKey = ProtocolUtils::key_proto_config_data(Proto::Awg);
    if (awgSrc.contains(awgDataKey)) {
        awgWrapper = awgSrc;
    } else if (!awgSrc.isEmpty()) {
        awgWrapper[awgDataKey] = awgSrc;
    }
    proto.awg = awgWrapper;
    proto.isThirdPartyConfig = true;

    // Wrap into the canonical Amnezia container shape.
    QJsonObject container;
    container[configKey::container] = ContainerUtils::containerToString(DockerContainer::QqDns);
    container[ProtocolUtils::protoToString(Proto::QqDns)] = proto.toJson();

    QJsonArray containers;
    containers.push_back(container);

    QJsonObject config;
    config[configKey::containers] = containers;
    config[configKey::defaultContainer] = ContainerUtils::containerToString(DockerContainer::QqDns);
    config[configKey::description] =
            description.isEmpty() ? m_serversRepository->nextAvailableServerName() : description;
    // Surface the first send-domain as the server card's host label.
    if (!proto.sendDomains.isEmpty()) {
        config[configKey::hostName] = proto.sendDomains.first().toString();
    }

    return config;
}

void ImportController::checkForMaliciousStrings(const QJsonObject &serverConfig, QString &warningText) const
{
    const QJsonArray &containers = serverConfig.value(configKey::containers).toArray();
    for (const QJsonValue &container : containers) {
        auto containerConfig = container.toObject();
        auto containerName = containerConfig[configKey::container].toString();
        if (containerName == ContainerUtils::containerToString(DockerContainer::OpenVpn)) {

            QString protocolConfig =
                    containerConfig[ProtocolUtils::protoToString(Proto::OpenVpn)].toObject()[configKey::lastConfig].toString();
            QString protocolConfigJson = QJsonDocument::fromJson(protocolConfig.toUtf8()).object()[configKey::config].toString();

            // https://github.com/OpenVPN/openvpn/blob/master/doc/man-sections/script-options.rst
            QStringList dangerousTags {
                "up", "tls-verify", "ipchange", "client-connect", "route-up", "route-pre-down", "client-disconnect", "down", "learn-address", "auth-user-pass-verify"
            };

            QStringList maliciousStrings;
            QStringList lines = protocolConfigJson.split('\n', Qt::SkipEmptyParts);

            for (const QString &rawLine : lines) {
                QString line = rawLine.trimmed();

                QString command = line.section(' ', 0, 0, QString::SectionSkipEmpty);
                if (dangerousTags.contains(command, Qt::CaseInsensitive)) {
                    maliciousStrings << rawLine;
                }
            }

            warningText = "This configuration contains an OpenVPN setup. OpenVPN configurations can include malicious "
                         "scripts, so only add it if you fully trust the provider of this config. ";

            if (!maliciousStrings.isEmpty()) {
                warningText += "<br>In the imported configuration, potentially dangerous lines were found:";
                for (const auto &string : maliciousStrings) {
                    warningText += QString("<br><i>%1</i>").arg(string);
                }
            }
        }
    }
}

void ImportController::processAmneziaConfig(QJsonObject &config) const
{
    auto containers = config.value(configKey::containers).toArray();
    for (auto i = 0; i < containers.size(); i++) {
        auto container = containers.at(i).toObject();
        auto dockerContainer = ContainerUtils::containerFromString(container.value(configKey::container).toString());
        if (ContainerUtils::isAwgContainer(dockerContainer) || dockerContainer == DockerContainer::WireGuard) {
            auto containerConfig = container.value(ContainerUtils::containerTypeToProtocolString(dockerContainer)).toObject();
            auto protocolConfig = containerConfig.value(configKey::lastConfig).toString();
            if (protocolConfig.isEmpty()) {
                return;
            }

            QJsonObject jsonConfig = QJsonDocument::fromJson(protocolConfig.toUtf8()).object();
            jsonConfig[configKey::mtu] =
                    ContainerUtils::isAwgContainer(dockerContainer) ? protocols::awg::defaultMtu : protocols::wireguard::defaultMtu;

            containerConfig[configKey::lastConfig] = QString(QJsonDocument(jsonConfig).toJson());

            container[ContainerUtils::containerTypeToProtocolString(dockerContainer)] = containerConfig;
            containers.replace(i, container);
            config.insert(configKey::containers, containers);
        }
    }
}

