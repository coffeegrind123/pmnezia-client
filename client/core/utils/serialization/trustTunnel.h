// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TRUSTTUNNEL_SERIALIZATION_H
#define TRUSTTUNNEL_SERIALIZATION_H

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <optional>

// TrustTunnel client-side formats, QtCore only so the privileged service can
// link it too:
//
//   * tt:// deep links (TrustTunnel DEEP_LINK.md): "tt://?" + base64url-nopad
//     of a TLV stream, each field varint(tag) || varint(len) || value, varints
//     per RFC 9000 §16.
//   * The [endpoint] TOML that trusttunnel_client reads, and the flat TOML the
//     endpoint's `-c <user> --format toml` export writes (same keys, no table
//     header, client_random spelled client_random_prefix).
//   * The client TOML handed to trusttunnel_client, rendered from an Endpoint.
namespace amnezia::serialization::trusttunnel
{
    // One endpoint profile. Field names follow the [endpoint] TOML keys.
    struct Endpoint
    {
        int version = 1;
        QString hostname;
        QStringList addresses;
        QString customSni;
        bool hasIpv6 = true;
        QString username;
        QString password;
        bool skipVerification = false;
        // PEM chain; empty means "verify against the system store".
        QString certificate;
        QString upstreamProtocol = QStringLiteral("http2");
        bool antiDpi = false;
        // "hex[/hexmask]"; empty for none.
        QString clientRandom;
        QString name;
        QStringList dnsUpstreams;

        // Deep-link v2 only. trusttunnel_client v1.1.x can use neither, so a
        // link that depends on them is rejected at connect, not at import.
        QString subscriptionUrl;
        QString clientRandomAuthKey;

        // Missing hostname / addresses / credentials. Empty when complete.
        QString missingRequiredField() const;

        QJsonObject toJson() const;
        static Endpoint fromJson(const QJsonObject &json);
    };

    // Accepts "tt://?<payload>" and the legacy "tt://<payload>".
    std::optional<Endpoint> decodeDeepLink(const QString &uri, QString *error = nullptr);

    // Version 1 unless a v2-only field is set; field order and omission rules
    // match upstream's encoder, so a round trip is byte-identical.
    QString encodeDeepLink(const Endpoint &endpoint);

    // Either TOML shape described above. Fails on malformed TOML or a missing
    // required field.
    std::optional<Endpoint> parseToml(const QString &toml, QString *error = nullptr);

    // True for text that carries the endpoint keys of either TOML shape.
    bool looksLikeEndpointToml(const QString &text);

    struct SocksListener
    {
        QString address; // "127.0.0.1:<port>"
        QString username;
        QString password;
    };

    // The full trusttunnel_client configuration: endpoint plus a SOCKS5
    // listener, kill switch off (the app enforces its own).
    QString buildClientToml(const Endpoint &endpoint, const SocksListener &socks);

    // A TOML basic string literal, quotes included.
    QString tomlString(const QString &value);

    // Deep links carry the certificate as concatenated DER; TOML and the CLI
    // want PEM. Both return empty on malformed input.
    QString derChainToPem(const QByteArray &der);
    QByteArray pemChainToDer(const QString &pem);

    namespace toml
    {
        // A deliberately small TOML reader: tables, bare/quoted keys, the four
        // string forms, booleans, integers, and (possibly multi-line) arrays of
        // those. Array-of-tables sections are skipped. Values land in a
        // QJsonObject per table; the root table is "".
        std::optional<QMap<QString, QJsonObject>> parse(const QString &text, QString *error = nullptr);
    }
}

#endif // TRUSTTUNNEL_SERIALIZATION_H
