// SPDX-License-Identifier: GPL-3.0-or-later
//
// Service-side half of the TrustTunnel transport. The client process cannot
// hand trusttunnel_client a config file directly: the client runs as root
// (PermittedProcess::TrustTunnelClient), so the TOML it reads is rendered here,
// from validated fields, into a path only the service account can read, and
// sanitizeArguments() accepts no other path for -c.

#ifndef TRUST_TUNNEL_SERVICE_H
#define TRUST_TUNNEL_SERVICE_H

#include <QString>

class TrustTunnelService
{
public:
    // Returns the written path, or an empty string when any field fails
    // validation or the write fails.
    static QString writeConfig(const QString &endpointJson, const QString &socksAddress,
                               const QString &socksUser, const QString &socksPassword);
    static bool removeConfig();
};

#endif // TRUST_TUNNEL_SERVICE_H
