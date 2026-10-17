// SPDX-License-Identifier: GPL-3.0-or-later

#include "trust_tunnel_service.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

#include "core/utils/serialization/trustTunnel.h"
#include "core/utils/utilities.h"

namespace tt = amnezia::serialization::trusttunnel;

namespace
{

constexpr qsizetype maxFieldLength = 1024;
constexpr qsizetype maxCertificateLength = 64 * 1024;

bool hasControlChars(const QString &s)
{
    for (const QChar c : s) {
        if (c.unicode() < 0x20 || c.unicode() == 0x7F) {
            return true;
        }
    }
    return false;
}

bool isPlainField(const QString &s)
{
    return s.size() <= maxFieldLength && !hasControlChars(s);
}

bool validEndpoint(const tt::Endpoint &e)
{
    if (!e.missingRequiredField().isEmpty() || e.hostname.isEmpty() || e.addresses.isEmpty()) {
        qWarning() << "TrustTunnelService: endpoint is missing hostname, addresses or credentials";
        return false;
    }

    // "host:port" or "[v6]:port"; trusttunnel_client also accepts a bare host
    // and applies its default port.
    static const QRegularExpression address(
            QStringLiteral("^(?:\\[[0-9A-Fa-f:.]+\\]|[A-Za-z0-9.-]+)(?::[0-9]{1,5})?$"));
    for (const QString &a : e.addresses) {
        if (!address.match(a).hasMatch()) {
            qWarning() << "TrustTunnelService: rejected endpoint address" << a;
            return false;
        }
    }

    QStringList fields { e.hostname, e.customSni, e.username, e.password, e.clientRandom, e.name };
    fields.append(e.dnsUpstreams);
    for (const QString &f : fields) {
        if (!isPlainField(f)) {
            qWarning() << "TrustTunnelService: rejected an over-long or control-character field";
            return false;
        }
    }

    if (e.certificate.size() > maxCertificateLength
        || (!e.certificate.isEmpty() && tt::pemChainToDer(e.certificate).isEmpty())) {
        qWarning() << "TrustTunnelService: certificate is not a PEM chain";
        return false;
    }
    return true;
}

bool validSocks(const QString &address, const QString &user, const QString &password)
{
    static const QRegularExpression loopback(QStringLiteral("^127\\.0\\.0\\.1:([0-9]{1,5})$"));
    const auto m = loopback.match(address);
    if (!m.hasMatch() || m.captured(1).toInt() < 1 || m.captured(1).toInt() > 65535) {
        qWarning() << "TrustTunnelService: SOCKS listener must be 127.0.0.1:<port>, got" << address;
        return false;
    }
    static const QRegularExpression token(QStringLiteral("^[A-Za-z0-9]{1,64}$"));
    if (!token.match(user).hasMatch() || !token.match(password).hasMatch()) {
        qWarning() << "TrustTunnelService: SOCKS credentials must be 1-64 alphanumerics";
        return false;
    }
    return true;
}

} // namespace

QString TrustTunnelService::writeConfig(const QString &endpointJson, const QString &socksAddress,
                                        const QString &socksUser, const QString &socksPassword)
{
    QJsonParseError err {};
    const QJsonDocument doc = QJsonDocument::fromJson(endpointJson.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "TrustTunnelService: endpoint is not a JSON object:" << err.errorString();
        return {};
    }

    const tt::Endpoint endpoint = tt::Endpoint::fromJson(doc.object());
    if (!validEndpoint(endpoint) || !validSocks(socksAddress, socksUser, socksPassword)) {
        return {};
    }

    const QString toml = tt::buildClientToml(endpoint, { socksAddress, socksUser, socksPassword });

    // Read the rendering back: anything the reader rejects, trusttunnel_client
    // would too, and only after it is already running as root.
    QString parseError;
    if (!tt::parseToml(toml, &parseError)) {
        qWarning() << "TrustTunnelService: rendered TOML does not parse:" << parseError;
        return {};
    }

    const QString path = Utils::trustTunnelConfigPath();
    const QString dir = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(dir)) {
        qWarning() << "TrustTunnelService: cannot create" << dir;
        return {};
    }
    QFile::setPermissions(dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "TrustTunnelService: cannot open" << path << file.errorString();
        return {};
    }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.write(toml.toUtf8());
    if (!file.commit()) {
        qWarning() << "TrustTunnelService: cannot write" << path << file.errorString();
        return {};
    }
    return path;
}

bool TrustTunnelService::removeConfig()
{
    const QString path = Utils::trustTunnelConfigPath();
    return !QFile::exists(path) || QFile::remove(path);
}
