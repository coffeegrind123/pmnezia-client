package org.amnezia.vpn.protocol.masterdnsvpn

import android.net.VpnService.Builder
import java.io.File
import java.io.IOException
import java.net.InetSocketAddress
import java.net.Socket
import org.amnezia.vpn.protocol.BadConfigException
import org.amnezia.vpn.protocol.Protocol
import org.amnezia.vpn.protocol.ProtocolState.CONNECTED
import org.amnezia.vpn.protocol.ProtocolState.DISCONNECTED
import org.amnezia.vpn.protocol.Statistics
import org.amnezia.vpn.protocol.VpnStartException
import org.amnezia.vpn.protocol.masterdnsvpn.libmasterdnsvpn.Libmasterdnsvpn
import org.amnezia.vpn.protocol.xray.libXray.LibXray
import org.amnezia.vpn.protocol.xray.libXray.Tun2SocksConfig
import org.amnezia.vpn.util.Log
import org.amnezia.vpn.util.net.InetNetwork
import org.amnezia.vpn.util.net.parseInetAddress
import org.json.JSONObject

private const val TAG = "MasterDnsVpn"
private const val LIB_TAG = "libMasterDnsVpn"

class MasterDnsVpn : Protocol() {

    private var isRunning: Boolean = false
    override val statistics: Statistics = Statistics.EMPTY_STATISTICS

    override fun internalInit() {
        // No persistent native init needed: the bundled mdnsvpn core is
        // started fresh on every connect and torn down on disconnect.
        // Logger initialisation lives inside the libMasterDnsVpn AAR.
    }

    override suspend fun startVpn(
        config: JSONObject,
        vpnBuilder: Builder,
        protect: (Int) -> Boolean
    ) {
        if (isRunning) {
            Log.w(TAG, "MasterDnsVpn already running")
            return
        }

        val configData = config.optJSONObject("masterdnsvpn_config_data")
            ?: throw BadConfigException("masterdnsvpn_config_data not found")

        val clientToml = configData.optString("config")
        if (clientToml.isBlank()) {
            throw BadConfigException(
                "masterdnsvpn client_config.toml is missing — operator hasn't generated a peer config"
            )
        }

        val mdnsvpnConfig = parseConfig(config, configData)

        // Write the operator-supplied client_config.toml to a private file.
        // mdnsvpn reads `RESOLVERS = […]` from inside the TOML so we don't
        // need a separate -resolvers file.
        val configDir = File(context.filesDir, "masterdnsvpn").apply { mkdirs() }
        val configFile = File(configDir, "client_config.toml")
        try {
            configFile.writeText(clientToml)
        } catch (e: IOException) {
            throw VpnStartException("Failed to write mdnsvpn config: ${e.message}")
        }

        Log.d(TAG, "Starting mdnsvpn core (port=${mdnsvpnConfig.socksPort})")
        val startErr = Libmasterdnsvpn.startClient(configFile.absolutePath)
        if (!startErr.isNullOrBlank()) {
            throw VpnStartException("Failed to start mdnsvpn core: $startErr")
        }

        // mdnsvpn does per-resolver MTU discovery before binding the SOCKS5
        // listener. Poll until either the listener accepts a TCP connect or
        // the deadline fires — tun2socks must not race the listener startup.
        if (!waitForSocksListener(mdnsvpnConfig.socksPort)) {
            stopMdnsvpnCore()
            throw VpnStartException(
                "mdnsvpn SOCKS5 listener did not bind 127.0.0.1:${mdnsvpnConfig.socksPort} within deadline"
            )
        }

        try {
            buildVpnInterface(mdnsvpnConfig, vpnBuilder)
            vpnBuilder.establish().use { tunFd ->
                if (tunFd == null) {
                    throw VpnStartException(
                        "Create VPN interface: permission not granted or revoked"
                    )
                }
                Log.d(TAG, "Run tun2Socks (SOCKS5 backend = mdnsvpn core)")
                runTun2Socks(mdnsvpnConfig, tunFd.detachFd())
            }
        } catch (e: Exception) {
            stopMdnsvpnCore()
            throw e
        }

        state.value = CONNECTED
        isRunning = true
    }

    override fun stopVpn() {
        LibXray.stopTun2Socks().isNotNullOrBlank { err ->
            Log.e(TAG, "Failed to stop tun2Socks: $err")
        }
        stopMdnsvpnCore()
        isRunning = false
        state.value = DISCONNECTED
    }

    override fun reconnectVpn(vpnBuilder: Builder, protect: (Int) -> Boolean) {
        // mdnsvpn handles its own retransmission + per-resolver failover via
        // its ARQ layer; from Amnezia's perspective the tunnel is up as long
        // as tun2socks and the core process are alive. No-op reconnect.
        state.value = CONNECTED
    }

    private fun parseConfig(config: JSONObject, configData: JSONObject): MasterDnsVpnConfig {
        return MasterDnsVpnConfig.build {
            addAddress(MasterDnsVpnConfig.DEFAULT_IPV4_ADDRESS)

            config.optString("dns1").let {
                if (it.isNotBlank()) addDnsServer(parseInetAddress(it))
            }
            config.optString("dns2").let {
                if (it.isNotBlank()) addDnsServer(parseInetAddress(it))
            }

            // Default-route both v4 and v6 — mdnsvpn is intended as a
            // full-tunnel transport (when only DNS leaves the network).
            addRoute(InetNetwork("0.0.0.0", 0))
            addRoute(InetNetwork("2000::0", 3))

            // Carve the operator's server hostname back out of the tunnel
            // so the underlying DNS queries can still reach it.
            config.optString("hostName").let {
                if (it.isNotBlank()) excludeRoute(InetNetwork(it, 32))
            }

            config.optString("mtu").let {
                if (it.isNotBlank()) setMtu(it.toInt())
            }

            // Per-peer SOCKS5 listen port chosen by the operator (default
            // 18000 mirrors the upstream sample).
            val listenPort = configData.optString("listenPort", "")
            val parsedPort = listenPort.toIntOrNull() ?: DEFAULT_SOCKS_PORT
            setSocksPort(parsedPort)

            configSplitTunneling(config)
            configAppSplitTunneling(config)
        }
    }

    private fun runTun2Socks(config: MasterDnsVpnConfig, fd: Int) {
        // The bundled mdnsvpn core listens locally without auth — the operator
        // controls the device-to-core hop, and the SOCKS5 listener is bound
        // to 127.0.0.1 only. We feed tun2socks an unauthenticated proxy URL.
        val proxyUrl = "socks5://127.0.0.1:${config.socksPort}"
        val tun2SocksConfig = Tun2SocksConfig().apply {
            mtu = config.mtu.toLong()
            proxy = proxyUrl
            device = "fd://$fd"
            logLevel = "warn"
        }
        LibXray.startTun2Socks(tun2SocksConfig, fd.toLong()).isNotNullOrBlank { err ->
            throw VpnStartException("Failed to start tun2socks for mdnsvpn: $err")
        }
    }

    private fun waitForSocksListener(port: Int): Boolean {
        val deadline = System.currentTimeMillis() + SOCKS_WAIT_TIMEOUT_MS
        while (System.currentTimeMillis() < deadline) {
            try {
                Socket().use { sock ->
                    sock.connect(InetSocketAddress("127.0.0.1", port), SOCKS_PROBE_TIMEOUT_MS)
                    if (sock.isConnected) {
                        Log.d(TAG, "mdnsvpn SOCKS5 listener up on 127.0.0.1:$port")
                        return true
                    }
                }
            } catch (_: IOException) {
                // not yet bound — retry after a short pause
                try {
                    Thread.sleep(SOCKS_PROBE_TIMEOUT_MS.toLong())
                } catch (_: InterruptedException) {
                    Thread.currentThread().interrupt()
                    return false
                }
            }
        }
        return false
    }

    private fun stopMdnsvpnCore() {
        Libmasterdnsvpn.stopClient().isNotNullOrBlank { err ->
            Log.e(TAG, "Failed to stop mdnsvpn core: $err")
        }
    }

    companion object {
        private const val DEFAULT_SOCKS_PORT = 18_000
        private const val SOCKS_WAIT_TIMEOUT_MS = 60_000
        private const val SOCKS_PROBE_TIMEOUT_MS = 250

        val instance: MasterDnsVpn by lazy { MasterDnsVpn() }
    }
}

private fun String?.isNotNullOrBlank(block: (String) -> Unit) {
    if (!this.isNullOrBlank()) {
        block(this)
    }
}
