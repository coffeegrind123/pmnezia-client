package org.amnezia.vpn.protocol.qqdns

import android.net.VpnService.Builder
import org.amnezia.vpn.protocol.BadConfigException
import org.amnezia.vpn.protocol.VpnStartException
import org.amnezia.vpn.protocol.wireguard.Wireguard
import org.amnezia.vpn.protocol.wireguard.WireguardConfig
import org.amnezia.vpn.util.Log
import org.amnezia.vpn.util.net.InetNetwork
import org.json.JSONObject

private const val TAG = "QqDns"

/**
 * Android-side glue for the QQ-DNS (UDP-over-DNS) transport.
 *
 * QQ-DNS is not a self-contained tunnel: the native engine binds a loopback
 * UDP port and **AmneziaWG runs on top of it**. So this class extends the WireGuard base
 * (reusing the whole WireGuard-for-Android backend) and only:
 *
 *   1. starts the native engine and learns its loopback UDP port,
 *   2. rewrites the embedded AmneziaWG endpoint to 127.0.0.1:<that port>, and
 *   3. excludes the resolver IPs from the tun so the engine's outbound DNS
 *      queries don't loop back into it (the WG endpoint is loopback and
 *      already bypasses the tun; the base also protect()s the WG socket).
 *
 * Everything else — TUN setup, wireguard-go turn-on, routing, split
 * tunnelling, stats — comes from the `Wireguard` base unchanged.
 *
 * KNOWN RUNTIME TODO: resolver exclusion relies on VpnService.Builder
 * excludeRoute (API 33+, as the MasterDnsVPN protocol also uses). On older
 * devices, or setups where that isn't honoured, the engine's resolver sockets
 * would need explicit protect() via their fds — needs on-device verification.
 */
class QqDns : Wireguard() {

    override val ifName: String = "qqdns0"

    private var localUdpPort: Int = 0
    private var resolverIps: List<String> = emptyList()

    override suspend fun startVpn(
        config: JSONObject,
        vpnBuilder: Builder,
        protect: (Int) -> Boolean
    ) {
        val wrapper = config.optJSONObject("qqdns_config_data")
            ?: throw BadConfigException("qqdns_config_data not found")

        if (!QqDnsNative.nativeStart(wrapper.toString())) {
            throw VpnStartException("Failed to start QqDns engine: ${QqDnsNative.nativeLastError()}")
        }

        localUdpPort = waitForLocalPort()
        if (localUdpPort == 0) {
            QqDnsNative.nativeStop()
            throw VpnStartException("QqDns engine did not bind a local UDP port")
        }
        Log.d(TAG, "QqDns engine listening on 127.0.0.1:$localUdpPort")

        resolverIps = extractResolverIps(wrapper)

        // Rewrite the embedded AmneziaWG endpoint to the loopback engine port,
        // then present a top-level awg_config_data for the Awg/Wireguard base.
        val awgConfigData = wrapper.getJSONObject("awg").getJSONObject("awg_config_data")
        awgConfigData.put("hostName", "127.0.0.1")
        awgConfigData.put("port", localUdpPort)

        val innerConfig = JSONObject(config.toString())
        innerConfig.put("awg_config_data", awgConfigData)

        try {
            super.startVpn(innerConfig, vpnBuilder, protect)
        } catch (e: Exception) {
            QqDnsNative.nativeStop()
            throw e
        }
    }

    override fun parseConfig(config: JSONObject): WireguardConfig {
        // Same as Awg.parseConfig, plus resolver exclude-routes so the engine's
        // DNS queries bypass the tun instead of looping into it.
        val configData = config.getJSONObject("awg_config_data")
        return WireguardConfig.build {
            setUseProtocolExtension(true)
            configExtensionParameters(configData)
            configWireguard(config, configData)
            configSplitTunneling(config)
            configAppSplitTunneling(config)
            resolverIps.forEach { ip -> excludeRoute(InetNetwork(ip, 32)) }
        }
    }

    override fun stopVpn() {
        super.stopVpn()
        QqDnsNative.nativeStop()
    }

    private fun waitForLocalPort(): Int {
        val deadline = System.currentTimeMillis() + PORT_WAIT_TIMEOUT_MS
        while (System.currentTimeMillis() < deadline) {
            val port = QqDnsNative.nativeLocalUdpPort()
            if (port > 0) return port
            try {
                Thread.sleep(PORT_POLL_INTERVAL_MS.toLong())
            } catch (_: InterruptedException) {
                Thread.currentThread().interrupt()
                return 0
            }
        }
        return 0
    }

    private fun extractResolverIps(wrapper: JSONObject): List<String> {
        val out = mutableListOf<String>()
        val arr = wrapper.optJSONArray("dns_ips") ?: return out
        for (i in 0 until arr.length()) {
            var ip = arr.optString(i).trim()
            if (ip.isEmpty()) continue
            // Strip "ip:port" (single colon); keep bare/bracketed IPv6.
            if (ip.startsWith("[")) {
                val c = ip.indexOf("]")
                if (c > 0) ip = ip.substring(1, c)
            } else if (ip.count { it == ':' } == 1) {
                ip = ip.substringBefore(":")
            }
            out.add(ip)
        }
        return out
    }

    companion object {
        private const val PORT_WAIT_TIMEOUT_MS = 60_000
        private const val PORT_POLL_INTERVAL_MS = 250

        val instance: QqDns by lazy { QqDns() }
    }
}
