@file:Suppress("UnstableApiUsage")

// Wraps the gomobile-built MasterDnsVPN AAR as a flat Gradle artifact so the
// sibling :master_dns_vpn module can depend on it via project(":master_dns_vpn:libMasterDnsVpn").
//
// The AAR itself is provisioned out of the conan cache by the build pipeline
// (see conanfile.py — `amnezia-mdnsvpn-android/<ver>` requires Android) and
// dropped into this directory as `libmasterdnsvpn.aar` before assemble runs.
configurations {
    maybeCreate("default")
}
artifacts.add("default", file("libmasterdnsvpn.aar"))
