plugins {
    id(libs.plugins.android.library.get().pluginId)
    id(libs.plugins.kotlin.android.get().pluginId)
}

kotlin {
    jvmToolchain(17)
}

android {
    namespace = "org.amnezia.vpn.protocol.trusttunnel"
}

dependencies {
    compileOnly(project(":utils"))
    compileOnly(project(":protocolApi"))
    // tun2socks from libxray.aar, as for Xray and MasterDnsVPN; it dials the
    // SOCKS5 listener trusttunnel_client opens on loopback.
    implementation(project(":xray:libXray"))
    implementation(libs.kotlinx.coroutines)
}
