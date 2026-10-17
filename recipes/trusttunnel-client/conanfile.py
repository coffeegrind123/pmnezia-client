from conan import ConanFile
from conan.tools.layout import basic_layout
from conan.tools.files import get, copy
from conan.errors import ConanInvalidConfiguration

import os


class TrustTunnelClient(ConanFile):
    """Upstream's prebuilt trusttunnel_client CLI (TrustTunnel/TrustTunnelClient).

    Not built from source: the CLI pulls AdGuard's own conan remote and a
    BoringSSL/OpenSSL fork that would clash with this tree's OpenSSL. The
    release binaries are self-contained (static musl on Linux and Android,
    system DLLs only on Windows), and each archive is pinned by the sha256
    GitHub records for the release asset.
    """

    name = "trusttunnel-client"
    version = "1.1.11"
    package_type = "application"
    settings = "os", "arch"

    # (os, arch) -> (release asset suffix, sha256, has top-level directory)
    _assets = {
        ("Linux", "x86_64"): ("linux-x86_64.tar.gz",
                              "00e0546a6b35840dbb92b741dff3da6732a74bf2d6c581e044bb4567c0d96825", True),
        ("Linux", "armv8"): ("linux-aarch64.tar.gz",
                             "880e9c8afd5cce7ff38c53978f0ea530ff95e679bcfef69c5127b26b3692d110", True),
        ("Windows", "x86_64"): ("windows-x86_64.zip",
                                "c6db992c1164abef1a19a8289173be23d87c6f8c039360dbe9b7fdef89a9e526", False),
        ("Windows", "armv8"): ("windows-aarch64.zip",
                               "8c6b3ad5d90db795e4f00f9545076c06c2bbd8d673c9888b8169bf460d658a85", False),
        ("Macos", "*"): ("macos-universal.tar.gz",
                         "e2c67aaea941fcdcfc24ec9868164462fcea8d1f731f6928c49bbef300dcf1a3", True),
        # Android runs the static Linux build as a child process (see
        # client/android/trusttunnel); there is no i686 Linux build for x86.
        ("Android", "armv8"): ("linux-aarch64.tar.gz",
                               "880e9c8afd5cce7ff38c53978f0ea530ff95e679bcfef69c5127b26b3692d110", True),
        ("Android", "armv7"): ("linux-armv7.tar.gz",
                               "90c032f1b6de3bc797f8d144582805b565190e56fd5636a5eed089d7458ac1bc", True),
        ("Android", "x86_64"): ("linux-x86_64.tar.gz",
                                "00e0546a6b35840dbb92b741dff3da6732a74bf2d6c581e044bb4567c0d96825", True),
    }

    @property
    def _asset(self):
        os_name = str(self.settings.os)
        if os_name == "Macos":
            return self._assets[("Macos", "*")]
        return self._assets.get((os_name, str(self.settings.arch)))

    @property
    def _binary_name(self):
        return "trusttunnel_client.exe" if str(self.settings.os) == "Windows" else "trusttunnel_client"

    def layout(self):
        basic_layout(self)

    def validate(self):
        if not self._asset:
            raise ConanInvalidConfiguration(
                f"{self.name} v{self.version} has no release build for {self.settings.os} {self.settings.arch}"
            )

    def source(self):
        pass

    def build(self):
        suffix, sha256, has_root = self._asset
        get(self, f"https://github.com/TrustTunnel/TrustTunnelClient/releases/download/v{self.version}/"
                  f"trusttunnel_client-v{self.version}-{suffix}",
            sha256=sha256, strip_root=has_root, destination=self.build_folder)

    def package(self):
        copy(self, self._binary_name, src=self.build_folder, dst=os.path.join(self.package_folder, "bin"))
        for lic in ("LICENSE", "LICENSE.txt"):
            copy(self, lic, src=self.build_folder, dst=os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        self.cpp_info.exe = True
        self.cpp_info.location = os.path.join(self.package_folder, "bin", self._binary_name)
        self.cpp_info.set_property("cmake_target_name", "trusttunnel::trusttunnel-client")
        self.cpp_info.set_property("cmake_extra_variables", {
            "TRUSTTUNNEL_CLIENT_PATH": self.cpp_info.location.replace("\\", "/")
        })
