package org.amnezia.vpn

/**
 * Android `VpnService` host for the TrustTunnel protocol. The lifecycle comes
 * from [AmneziaVpnService]; the protocol itself (trusttunnel_client child
 * process plus tun2socks) lives in the `trusttunnel` module.
 */
class TrustTunnelService : AmneziaVpnService()
