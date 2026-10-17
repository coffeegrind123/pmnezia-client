package org.amnezia.vpn.protocol.trusttunnel

import android.net.VpnService.Builder
import android.util.Base64
import java.io.File
import java.net.Inet6Address
import java.net.InetAddress
import java.net.ServerSocket
import java.security.KeyStore
import java.security.SecureRandom
import java.security.cert.X509Certificate
import java.util.concurrent.TimeUnit
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.amnezia.vpn.protocol.BadConfigException
import org.amnezia.vpn.protocol.Protocol
import org.amnezia.vpn.protocol.ProtocolState.CONNECTED
import org.amnezia.vpn.protocol.ProtocolState.DISCONNECTED
import org.amnezia.vpn.protocol.Statistics
import org.amnezia.vpn.protocol.VpnStartException
import org.amnezia.vpn.protocol.xray.libXray.LibXray
import org.amnezia.vpn.protocol.xray.libXray.Tun2SocksConfig
import org.amnezia.vpn.util.Log
import org.amnezia.vpn.util.net.InetNetwork
import org.amnezia.vpn.util.net.parseInetAddress
import org.json.JSONArray
import org.json.JSONObject

private const val TAG = "TrustTunnel"

// Packaged as lib*.so so the installer extracts it to nativeLibraryDir, the
// one directory an app may exec from (client/cmake/android.cmake).
private const val CLIENT_BINARY = "libtrusttunnel_client.so"

// trusttunnel_client logs this on VPN_SS_CONNECTED (v1.1.11).
private const val CONNECTED_LOG_LINE = "Successfully connected to endpoint"

private const val SPLIT_TUNNEL_INCLUDE = 1

/**
 * TrustTunnel on Android: upstream's static Linux trusttunnel_client runs as a
 * child process in SOCKS5 mode, and libXray's tun2socks carries the VpnService
 * TUN into it.
 *
 *   apps -> TUN fd -> tun2socks -> socks5://127.0.0.1:<port> -> trusttunnel_client
 *        -> HTTP/2 or HTTP/3 -> endpoint
 *
 * The child cannot call VpnService.protect(), so the app's own UID is left out
 * of the VPN (addDisallowedApplication): its sockets, the child's included,
 * use the underlying network. tun2socks never sends to the network itself; it
 * reads the TUN fd and dials loopback.
 *
 * The binary is static musl and Android has no /etc/resolv.conf or
 * /etc/ssl/certs, so endpoint addresses are resolved here first and the
 * system CA store is exported for it as a PEM bundle (SSL_CERT_FILE).
 */
class TrustTunnel : Protocol() {

    private var process: Process? = null
    private var outputThread: Thread? = null
    @Volatile
    private var connectedSignal: Boolean = false
    private val outputTail = ArrayDeque<String>()
    private var isRunning: Boolean = false

    override val statistics: Statistics = Statistics.EMPTY_STATISTICS

    override fun internalInit() {}

    override suspend fun startVpn(config: JSONObject, vpnBuilder: Builder, protect: (Int) -> Boolean) {
        if (isRunning) {
            Log.w(TAG, "TrustTunnel already running")
            return
        }

        val endpoint = config.optJSONObject("trusttunnel_config_data")
            ?: throw BadConfigException("trusttunnel_config_data not found")
        val hostname = endpoint.optString("hostname")
        val addresses = endpoint.optJSONArray("addresses").toStringList()
        if (hostname.isBlank() || addresses.isEmpty()) {
            throw BadConfigException("TrustTunnel profile has no endpoint hostname or addresses")
        }
        if (endpoint.optString("username").isBlank() || endpoint.optString("password").isBlank()) {
            throw BadConfigException("TrustTunnel profile has no credentials")
        }

        val resolved = withContext(Dispatchers.IO) { resolveAddresses(addresses) }
        if (resolved.isEmpty()) {
            throw VpnStartException("Could not resolve any TrustTunnel endpoint address")
        }

        val socksPort = withContext(Dispatchers.IO) { ServerSocket(0, 1, InetAddress.getLoopbackAddress()).use { it.localPort } }
        val socksUser = randomToken()
        val socksPassword = randomToken()

        val workDir = File(context.noBackupFilesDir, "trusttunnel").apply { mkdirs() }
        val configFile = File(workDir, "trusttunnel_client.toml")
        writePrivate(configFile, buildToml(endpoint, resolved, socksPort, socksUser, socksPassword))

        val needsSystemCas = endpoint.optString("certificate").isBlank() && !endpoint.optBoolean("skip_verification")
        val caFile = File(workDir, "ca-bundle.pem")
        if (needsSystemCas) {
            writePrivate(caFile, systemCaBundle())
        }

        var startupSucceeded = false
        try {
            withContext(Dispatchers.IO) { launchClient(configFile, if (needsSystemCas) caFile else null) }

            val ttConfig = TrustTunnelConfig.build {
                addAddress(TrustTunnelConfig.DEFAULT_IPV4_ADDRESS)

                config.optString("dns1").let {
                    if (it.isNotBlank()) addDnsServer(parseInetAddress(it))
                }
                config.optString("dns2").let {
                    if (it.isNotBlank()) addDnsServer(parseInetAddress(it))
                }

                addRoute(InetNetwork("0.0.0.0", 0))
                if (endpoint.optBoolean("has_ipv6", true)) {
                    addRoute(InetNetwork("2000::0", 3))
                }

                config.optString("mtu").let {
                    if (it.isNotBlank()) setMtu(it.toInt())
                }

                setSocks(socksPort, socksUser, socksPassword)

                configSplitTunneling(config)
                configAppSplitTunneling(config)
                // In include mode the app is already outside the VPN, and
                // VpnService.Builder rejects mixing allowed and disallowed apps.
                if (config.optInt("appSplitTunnelType") != SPLIT_TUNNEL_INCLUDE) {
                    excludeApplication(context.packageName)
                }
            }

            buildVpnInterface(ttConfig, vpnBuilder)
            vpnBuilder.establish().use { tunFd ->
                if (tunFd == null) {
                    throw VpnStartException("Create VPN interface: permission not granted or revoked")
                }
                runTun2Socks(ttConfig, tunFd.detachFd())
            }
            startupSucceeded = true
        } finally {
            if (!startupSucceeded) {
                stopClient()
                configFile.delete()
            }
        }

        state.value = CONNECTED
        isRunning = true
    }

    override fun stopVpn() {
        LibXray.stopTun2Socks().let { err ->
            if (!err.isNullOrBlank()) Log.e(TAG, "Failed to stop tun2socks: $err")
        }
        stopClient()
        // The TOML carries the endpoint credentials; it lives only while connected.
        File(File(context.noBackupFilesDir, "trusttunnel"), "trusttunnel_client.toml").delete()
        isRunning = false
        state.value = DISCONNECTED
    }

    override fun reconnectVpn(vpnBuilder: Builder, protect: (Int) -> Boolean) {
        // trusttunnel_client recovers the session itself; while it and
        // tun2socks are alive the tunnel is up.
        state.value = CONNECTED
    }

    private fun launchClient(configFile: File, caFile: File?) {
        val binary = File(context.applicationInfo.nativeLibraryDir, CLIENT_BINARY)
        if (!binary.canExecute()) {
            throw VpnStartException("trusttunnel_client is not bundled for this ABI")
        }

        connectedSignal = false
        synchronized(outputTail) { outputTail.clear() }

        val pb = ProcessBuilder(binary.absolutePath, "-c", configFile.absolutePath)
            .directory(configFile.parentFile)
            .redirectErrorStream(true)
        caFile?.let { pb.environment()["SSL_CERT_FILE"] = it.absolutePath }
        val proc = pb.start()
        process = proc

        outputThread = Thread {
            proc.inputStream.bufferedReader().useLines { lines ->
                for (line in lines) {
                    // Per-connection lines name destinations; keep them out of the log.
                    if (!line.contains(". Action: ")) {
                        Log.d(TAG, line)
                    }
                    synchronized(outputTail) {
                        outputTail.addLast(line)
                        while (outputTail.size > 20) outputTail.removeFirst()
                    }
                    if (line.contains(CONNECTED_LOG_LINE)) {
                        connectedSignal = true
                    }
                }
            }
            if (isRunning) {
                Log.e(TAG, "trusttunnel_client exited: ${lastOutput()}")
                onError("TrustTunnel client exited")
            }
        }.apply {
            name = "trusttunnel-output"
            isDaemon = true
            start()
        }

        val deadline = System.currentTimeMillis() + CONNECT_TIMEOUT_MS
        while (System.currentTimeMillis() < deadline) {
            if (connectedSignal) return
            if (!proc.isAlive) {
                throw VpnStartException("TrustTunnel client could not connect: ${lastOutput()}")
            }
            Thread.sleep(POLL_INTERVAL_MS)
        }
        throw VpnStartException("TrustTunnel client did not connect within ${CONNECT_TIMEOUT_MS / 1000} s: ${lastOutput()}")
    }

    private fun stopClient() {
        val proc = process ?: return
        process = null
        // SIGTERM: trusttunnel_client disconnects cleanly on it.
        proc.destroy()
        if (!proc.waitFor(2, TimeUnit.SECONDS)) {
            proc.destroyForcibly()
        }
        outputThread = null
    }

    private fun lastOutput(): String = synchronized(outputTail) { outputTail.joinToString(" | ") }

    private fun runTun2Socks(config: TrustTunnelConfig, fd: Int) {
        val tun2SocksConfig = Tun2SocksConfig().apply {
            mtu = config.mtu.toLong()
            proxy = "socks5://${config.socksUser}:${config.socksPassword}@127.0.0.1:${config.socksPort}"
            device = "fd://$fd"
            logLevel = "warn"
        }
        LibXray.startTun2Socks(tun2SocksConfig, fd.toLong()).let { err ->
            if (!err.isNullOrBlank()) throw VpnStartException("Failed to start tun2socks for TrustTunnel: $err")
        }
    }

    companion object {
        private const val CONNECT_TIMEOUT_MS = 30_000L
        private const val POLL_INTERVAL_MS = 200L

        val instance: TrustTunnel by lazy { TrustTunnel() }
    }
}

private fun JSONArray?.toStringList(): List<String> {
    if (this == null) return emptyList()
    return (0 until length()).map { optString(it) }.filter { it.isNotBlank() }
}

// "host:port" / "[v6]:port" / "host" -> resolved "ip:port", in order, skipping
// what does not resolve.
internal fun resolveAddresses(addresses: List<String>): List<String> = addresses.mapNotNull { address ->
    val (host, portSuffix) = when {
        address.startsWith("[") && address.contains("]") ->
            address.substring(1, address.indexOf(']')) to address.substring(address.indexOf(']') + 1)
        address.count { it == ':' } == 1 ->
            address.substringBefore(':') to ":" + address.substringAfter(':')
        else -> address to ""
    }
    try {
        val ip = InetAddress.getByName(host)
        val text = ip.hostAddress ?: return@mapNotNull null
        (if (ip is Inet6Address) "[$text]" else text) + portSuffix
    } catch (e: Exception) {
        Log.w(TAG, "Could not resolve TrustTunnel endpoint $host: $e")
        null
    }
}

private fun randomToken(): String {
    val alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
    val random = SecureRandom()
    return (1..16).map { alphabet[random.nextInt(alphabet.length)] }.joinToString("")
}

private fun writePrivate(file: File, text: String) {
    val tmp = File(file.parentFile, file.name + ".partial")
    tmp.writeText(text)
    tmp.setReadable(false, false)
    tmp.setReadable(true, true)
    tmp.setWritable(false, false)
    tmp.setWritable(true, true)
    if (!tmp.renameTo(file)) {
        tmp.delete()
        throw VpnStartException("Could not write ${file.name}")
    }
}

// Same escaping as serialization::trusttunnel::tomlString on the C++ side.
internal fun tomlString(value: String): String = buildString {
    append('"')
    for (c in value) {
        when {
            c == '"' -> append("\\\"")
            c == '\\' -> append("\\\\")
            c == '\n' -> append("\\n")
            c == '\r' -> append("\\r")
            c == '\t' -> append("\\t")
            c == '\b' -> append("\\b")
            c == '\u000C' -> append("\\f")
            c.code < 0x20 || c.code == 0x7F -> append("\\u%04x".format(c.code))
            else -> append(c)
        }
    }
    append('"')
}

private fun tomlArray(values: List<String>): String = values.joinToString(", ", "[", "]") { tomlString(it) }

internal fun buildToml(
    endpoint: JSONObject,
    addresses: List<String>,
    socksPort: Int,
    socksUser: String,
    socksPassword: String
): String = buildString {
    val upstream = if (endpoint.optString("upstream_protocol") == "http3") "http3" else "http2"
    appendLine("loglevel = \"info\"")
    appendLine("vpn_mode = \"general\"")
    appendLine("killswitch_enabled = false")
    appendLine("post_quantum_group_enabled = true")
    appendLine("exclusions = []")
    appendLine()
    appendLine("[endpoint]")
    appendLine("hostname = ${tomlString(endpoint.optString("hostname"))}")
    appendLine("addresses = ${tomlArray(addresses)}")
    endpoint.optString("custom_sni").takeIf { it.isNotBlank() }?.let { appendLine("custom_sni = ${tomlString(it)}") }
    appendLine("has_ipv6 = ${endpoint.optBoolean("has_ipv6", true)}")
    appendLine("username = ${tomlString(endpoint.optString("username"))}")
    appendLine("password = ${tomlString(endpoint.optString("password"))}")
    appendLine("client_random = ${tomlString(endpoint.optString("client_random"))}")
    appendLine("skip_verification = ${endpoint.optBoolean("skip_verification")}")
    appendLine("certificate = ${tomlString(endpoint.optString("certificate"))}")
    appendLine("upstream_protocol = ${tomlString(upstream)}")
    appendLine("anti_dpi = ${endpoint.optBoolean("anti_dpi")}")
    appendLine("dns_upstreams = ${tomlArray(endpoint.optJSONArray("dns_upstreams").toStringList())}")
    appendLine()
    appendLine("[listener.socks]")
    appendLine("address = ${tomlString("127.0.0.1:$socksPort")}")
    appendLine("username = ${tomlString(socksUser)}")
    appendLine("password = ${tomlString(socksPassword)}")
}

// The device's trusted CAs (system plus user-installed) as one PEM file. The
// static client otherwise finds no CA store and rejects every CA-signed endpoint.
private fun systemCaBundle(): String {
    val store = KeyStore.getInstance("AndroidCAStore").apply { load(null) }
    return buildString {
        for (alias in store.aliases()) {
            val cert = store.getCertificate(alias) as? X509Certificate ?: continue
            appendLine("-----BEGIN CERTIFICATE-----")
            Base64.encodeToString(cert.encoded, Base64.NO_WRAP).chunked(64).forEach(::appendLine)
            appendLine("-----END CERTIFICATE-----")
        }
    }
}
