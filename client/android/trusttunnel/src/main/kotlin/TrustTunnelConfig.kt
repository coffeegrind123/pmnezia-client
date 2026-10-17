package org.amnezia.vpn.protocol.trusttunnel

import org.amnezia.vpn.protocol.ProtocolConfig
import org.amnezia.vpn.util.net.InetNetwork

private const val TRUST_TUNNEL_DEFAULT_MTU = 1500

/**
 * VpnService interface parameters for one TrustTunnel session. The SOCKS5
 * listener is chosen per session, so it is filled in at start.
 */
class TrustTunnelConfig protected constructor(
    protocolConfigBuilder: ProtocolConfig.Builder,
    val socksPort: Int,
    val socksUser: String,
    val socksPassword: String,
) : ProtocolConfig(protocolConfigBuilder) {

    protected constructor(builder: Builder) : this(
        builder,
        builder.socksPort,
        builder.socksUser,
        builder.socksPassword,
    )

    class Builder : ProtocolConfig.Builder(false) {
        internal var socksPort: Int = 0
            private set
        internal var socksUser: String = ""
            private set
        internal var socksPassword: String = ""
            private set

        override var mtu: Int = TRUST_TUNNEL_DEFAULT_MTU

        fun setSocks(port: Int, user: String, password: String) = apply {
            socksPort = port
            socksUser = user
            socksPassword = password
        }

        override fun build(): TrustTunnelConfig =
            configBuild().run { TrustTunnelConfig(this@Builder) }
    }

    companion object {
        // Own /30, clear of Xray's 10.33 and MasterDnsVPN's 10.0.43.
        internal val DEFAULT_IPV4_ADDRESS: InetNetwork = InetNetwork("10.35.0.2", 30)

        inline fun build(block: Builder.() -> Unit): TrustTunnelConfig =
            Builder().apply(block).build()
    }
}
