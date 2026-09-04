#ifndef IMPORTCONTROLLER_H
#define IMPORTCONTROLLER_H

#include <QObject>
#include <QJsonObject>
#include <QByteArray>
#include <QMap>

#include "core/repositories/secureServersRepository.h"
#include "core/repositories/secureAppSettingsRepository.h"
#include "core/utils/errorCodes.h"
#include "core/utils/routeModes.h"
#include "core/utils/commonStructs.h"

namespace
{
    enum class ConfigTypes {
        Amnezia,
        WireGuard,
        Awg,
        Xray,
        MasterDnsVpn,
        QqDns,
        Backup,
        Invalid
    };
}

using namespace amnezia;

class ImportController : public QObject
{
    Q_OBJECT

public:
    struct ImportResult
    {
        ErrorCode errorCode = ErrorCode::NoError;
        QJsonObject config;
        QString configFileName;
        ConfigTypes configType = ConfigTypes::Invalid;
        bool isNativeWireGuardConfig = false;
    };

    explicit ImportController(SecureServersRepository* serversRepository,
                              SecureAppSettingsRepository* appSettingsRepository,
                              QObject *parent = nullptr);

    struct QrParseResult {
        bool success = false;
        ImportResult importResult;
        int chunksReceived = 0;
        int chunksTotal = 0;
    };

    ImportResult extractConfigFromData(const QString &data, const QString &configFileName = "");
    ImportResult extractConfigFromQr(const QByteArray &data);

    void startDecodingQr();
    QrParseResult parseQrCodeChunk(const QString &code);
    bool isQrDecodingActive() const;
    int qrChunksReceived() const;
    int qrChunksTotal() const;

    void importConfig(const QJsonObject &config);
    QJsonObject processNativeWireGuardConfig(const QJsonObject &config);

signals:
    void importFinished();
    void importErrorOccurred(ErrorCode errorCode, bool goToPageHome);
    void restoreAppConfig(const QByteArray &data);

private:
    ConfigTypes checkConfigFormat(const QString &config) const;
    QJsonObject extractWireGuardConfig(const QString &data, ConfigTypes &configType) const;
    QJsonObject extractXrayConfig(const QString &data, const QString &description = "") const;
    // Build an Amnezia server config from a MasterDnsVPN client_config JSON
    // (the upstream UPPER_SNAKE schema produced by coffeeblack-vpn's
    // `mdnsvpn://b64?` share blob and `mdnsvpn -json_base64`). `data` is the
    // decoded JSON string. `urlResolvers` carries the resolver list from the
    // share URL's `&resolvers=` parameter: upstream tags Resolvers `toml:"-"`
    // and its JSON loader skips such fields, so the server no longer emits a
    // RESOLVERS member and the URL is the only place the list survives.
    QJsonObject extractMasterDnsVpnConfig(const QString &data, const QString &description = "",
                                          const QStringList &urlResolvers = {}) const;
    // QQ-DNS (UDP-over-DNS) import: a JSON object with a "qqdns" transport block
    // and an embedded "awg" AmneziaWG config. Builds the canonical container.
    QJsonObject extractQqDnsConfig(const QString &data, const QString &description = "") const;
    void processAmneziaConfig(QJsonObject &config) const;

    SecureServersRepository* m_serversRepository;
    SecureAppSettingsRepository* m_appSettingsRepository;

    QMap<int, QByteArray> m_qrCodeChunks;
    bool m_isQrCodeProcessed = false;
    int m_totalQrCodeChunksCount = 0;
    int m_receivedQrCodeChunksCount = 0;
};

#endif // IMPORTCONTROLLER_H
