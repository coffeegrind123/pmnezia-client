package org.amnezia.vpn

/**
 * Android `VpnService` host for the QQ-DNS (UDP-over-DNS) protocol.
 *
 * Inherits the full lifecycle from [AmneziaVpnService] and only exists so the
 * platform can resolve a class name in the manifest. The actual work — the
 * native engine plus AmneziaWG running on top of its loopback UDP port — lives
 * in the `qqdns` module, owned by the singleton `QqDns` the parent dispatches to.
 */
class QqDnsService : AmneziaVpnService()
