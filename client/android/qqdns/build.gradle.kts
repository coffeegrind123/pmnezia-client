plugins {
    id(libs.plugins.android.library.get().pluginId)
    id(libs.plugins.kotlin.android.get().pluginId)
}

kotlin {
    jvmToolchain(17)
}

android {
    namespace = "org.amnezia.vpn.protocol.qqdns"
}

dependencies {
    compileOnly(project(":utils"))
    compileOnly(project(":protocolApi"))
    // QQ-DNS runs AmneziaWG on top of a loopback UDP port the native engine
    // binds, so it extends the AWG protocol and reuses the WireGuard backend.
    implementation(project(":wireguard"))
    implementation(libs.kotlinx.coroutines)
}
