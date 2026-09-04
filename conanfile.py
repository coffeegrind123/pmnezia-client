from conan import ConanFile

class AmneziaVPN(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "VirtualBuildEnv", "CMakeConfigDeps"

    options = {
        "macos_ne": [True, False]
    }
    default_options = {
        "macos_ne": False
    }

    def requirements(self):
        os = str(self.settings.os)

        has_ne = os == "iOS" or (os == "Macos" and self.options.macos_ne)
        has_service = os == "Windows" or os == "Linux" or (os == "Macos" and not has_ne)

        if has_service:
            if os == "Windows":
                self.requires("awg-windows/3.1.20260814")
                self.requires("tap-windows6/9.27.0")
                self.requires("win-split-tunnel/1.2.5.0")
                self.requires("wintun/0.14.1")
            else:
                self.requires("awg-go/3.1.20260814")

            self.requires("amnezia-xray-bindings/1.4.0")
            self.requires("tun2socks/2.6.0")
            self.requires("openvpn/2.7.0")
            self.requires("v2ray-rules-dat/202603162227")

        if has_ne:
            self.requires("awg-apple/3.1.4")
            self.requires("hev-socks5-tunnel/2.15.0", options={"as_framework": True})
            self.requires("openvpnadapter/1.0.0")

        if os == "Android":
            self.requires("amnezia-libxray/1.0.3")
            self.requires("awg-android/3.1.20260814")
            self.requires("openvpn-pt-android/1.0.0")

        self.requires("openssl/3.6.2")
        self.requires("zlib/1.3.2")

        # MasterDnsVPN engine (§8) — compression codecs negotiated via
        # SESSION_ACCEPT's compression pair byte. Server-selectable per
        # direction; clients ship/accept any of OFF (1), ZSTD (2), LZ4 (3),
        # ZLIB-raw-deflate (4). zlib is already a hard dep above; zstd and
        # lz4 are added here so the codec path mirrors upstream's
        # internal/compression/types.go feature surface.
        self.requires("zstd/1.5.6")
        self.requires("lz4/1.10.0")
