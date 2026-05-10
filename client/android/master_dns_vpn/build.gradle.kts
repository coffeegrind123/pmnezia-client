plugins {
    id(libs.plugins.android.library.get().pluginId)
    id(libs.plugins.kotlin.android.get().pluginId)
}

kotlin {
    jvmToolchain(17)
}

android {
    namespace = "org.amnezia.vpn.protocol.masterdnsvpn"
}

dependencies {
    compileOnly(project(":utils"))
    compileOnly(project(":protocolApi"))
    // Reuse the libXray AAR's bundled tun2socks engine + DialerController for
    // the SOCKS5 -> TUN bridge — the mdnsvpn Go core only ships a SOCKS5
    // listener, so a separate tun2socks layer is what carries packets back to
    // the Android VpnService TUN file descriptor.
    implementation(project(":xray:libXray"))
    implementation(project(":master_dns_vpn:libMasterDnsVpn"))
    implementation(libs.kotlinx.coroutines)
}
