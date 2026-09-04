#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include <QJsonObject>
#include "transfer.h"

namespace amnezia::serialization
{
    namespace vless
    {
        QJsonObject Deserialize(const QString &vless, QString *alias, QString *errMessage);
        const QString Serialize(const VlessServerObject &server, const QString &alias);
    } // namespace vless

    namespace inbounds
    {
        struct InboundCredentials {
            QString username;
            QString password;
            int port;
        };

        QJsonObject GenerateInboundEntry();

        // Reads existing SOCKS5 auth from the first inbound with protocol "socks"
        // (.settings.accounts[0]). Returns empty username/password if none.
        InboundCredentials GetInboundCredentials(const QJsonObject &xrayConfig);

        // Ensures SOCKS5 auth is present on the inbound whose protocol is "socks".
        // Re-uses existing credentials if already set; otherwise generates random ones
        // and writes them into the config. Assigns a free loopback TCP port each session
        // (OS-assigned). Throws std::runtime_error if a SOCKS inbound exists but binding
        // a local port on 127.0.0.1 fails (e.g. permissions or OS error).
        InboundCredentials EnsureInboundAuth(QJsonObject &xrayConfig);
    }
}

#endif // SERIALIZATION_H
