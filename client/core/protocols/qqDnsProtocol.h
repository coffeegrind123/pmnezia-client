// SPDX-License-Identifier: GPL-3.0-or-later
//
// QqDnsProtocol — the QQ-DNS (UDP-over-DNS) transport.
//
// Unlike MasterDnsVpnProtocol (which exposes a SOCKS5 port bridged by
// tun2socks), the QQ-DNS engine binds a loopback UDP port and **AmneziaWG runs
// on top of it**. So this protocol composes *under* Awg: it starts the engine
// in the privileged service (IPC), learns the local UDP port, rewrites an
// embedded AmneziaWG config's endpoint to 127.0.0.1:<port>, and delegates to an
// inner Awg instance — reusing all of AmneziaWG's TUN / routing / killswitch.
//
// Using a loopback endpoint sidesteps the usual VPN routing-loop for the WG
// handshake (loopback is never routed through the TUN).
//
// KNOWN RUNTIME TODO: once the inner Awg routes 0.0.0.0/0 through its TUN, the
// service-side engine's *outbound queries to public resolvers* must be exempted
// from the tunnel (bound to the physical interface / excluded from the WG
// routes), or they loop back into the tunnel. This needs on-device routing work
// and testing; it is not solvable at compile time.

#ifndef QQDNSPROTOCOL_H
#define QQDNSPROTOCOL_H

#include <QJsonObject>
#include <QPointer>
#include <QStringList>

#include "core/utils/errorCodes.h"
#include "vpnProtocol.h"

class Awg;

class QqDnsProtocol : public VpnProtocol
{
    Q_OBJECT

public:
    QqDnsProtocol(const QJsonObject &configuration, QObject *parent = nullptr);
    ~QqDnsProtocol() override;

    ErrorCode start() override;
    void stop() override;

private:
    // Build the inner AmneziaWG rawConfig from the outer config, swapping the
    // protocol to awg and rewriting the endpoint to 127.0.0.1:<enginePort>.
    QJsonObject buildInnerAwgConfig(quint16 enginePort) const;

    // Pin a /32 route for each resolver via the physical gateway so the
    // service-side engine's outbound DNS queries bypass the inner Awg's
    // 0.0.0.0/0 route instead of looping back into the tunnel. Runs once, when
    // the inner Awg reaches Connected (so it wins over Awg's freshly-set routes).
    void addResolverRouteExemptions();

    QJsonObject m_engineConfig;   // the qqdns wrapper (engine reads it directly)
    QPointer<Awg> m_awg;          // inner AmneziaWG, endpoint on the loopback engine
    QStringList m_resolverRoutes; // "ip/32" per resolver, for the exemption
    bool m_resolverRoutesAdded = false;
};

#endif // QQDNSPROTOCOL_H
