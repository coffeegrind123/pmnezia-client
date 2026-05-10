from conan import ConanFile
from conan.tools.files import get, copy
from conan.tools.layout import basic_layout
from conan.errors import ConanInvalidConfiguration

import os

# Desktop binary distribution of the MasterDnsVPN client. The privileged
# AmneziaVPN service spawns this binary as
#
#     mdnsvpn -config <path-to-toml> -nowait
#
# from masterDnsVpnProtocol::startMdnsvpn(). The release tarballs come
# pre-built from upstream — we don't compile from source here because the
# binary is statically linked Go and the upstream CI pipeline already does
# the platform-spread the project needs (Linux AMD64, Windows AMD64, macOS).


class AmneziaMdnsvpn(ConanFile):
    name = "amnezia-mdnsvpn"
    version = "2026.05.10"
    package_type = "application"
    settings = "os", "arch"

    # Pinned upstream tag (https://github.com/masterking32/MasterDnsVPN/releases).
    # The tag string is dotted-date + short-SHA — keep these in sync with the
    # SHA256SUMS.txt entries below when bumping.
    _upstream_tag = "v2026.05.10.180256-27c7e11"

    # SHA-256 of the release tarball (`MasterDnsVPN_Client_<Platform>.tar.gz`)
    # for each (os, arch) we deploy. Sourced verbatim from upstream's
    # SHA256SUMS.txt.
    _release_sha256 = {
        ("Linux", "x86_64"): (
            "MasterDnsVPN_Client_Linux_AMD64.tar.gz",
            "4b06a156f3a3dd73aecbf6f90ca4489ab0a567ffc69a57a45fc2a7018ed96ab9",
        ),
        ("Windows", "x86_64"): (
            "MasterDnsVPN_Client_Windows_AMD64.zip",
            # populated when bumping the recipe; placeholder is a hard fail.
            None,
        ),
        ("Macos", "x86_64"): (
            "MasterDnsVPN_Client_MacOS_AMD64.tar.gz",
            None,
        ),
        ("Macos", "armv8"): (
            "MasterDnsVPN_Client_MacOS_ARM64.tar.gz",
            None,
        ),
    }

    @property
    def _is_windows(self):
        return str(self.settings.get_safe("os")).startswith("Windows")

    @property
    def _ext(self):
        return ".exe" if self._is_windows else ""

    def layout(self):
        basic_layout(self, build_folder=".")

    def validate(self):
        key = (str(self.settings.os), str(self.settings.arch))
        if key not in self._release_sha256:
            raise ConanInvalidConfiguration(
                f"{self.name} v{self.version} does not support "
                f"{self.settings.os} {self.settings.arch}"
            )
        _, sha = self._release_sha256[key]
        if sha is None:
            raise ConanInvalidConfiguration(
                f"{self.name} v{self.version} does not yet have a pinned SHA-256 "
                f"for {self.settings.os}/{self.settings.arch}; populate "
                f"_release_sha256 in the recipe before bumping the version."
            )

    def source(self):
        key = (str(self.settings.os), str(self.settings.arch))
        asset, sha = self._release_sha256[key]
        url = (
            "https://github.com/masterking32/MasterDnsVPN/releases/download/"
            f"{self._upstream_tag}/{asset}"
        )
        # The tarballs ship a single ELF named after the version
        # (MasterDnsVPN_Client_Linux_AMD64_<tag>) plus the upstream sample
        # client_config.toml.simple. We extract everything; package() picks
        # out only the binary.
        get(self, url, sha256=sha, strip_root=False)

    def package(self):
        # The upstream tarball names the executable
        # `MasterDnsVPN_Client_<platform>_<tag>` (or .exe on Windows). We
        # rename to `mdnsvpn` so the privileged service's
        # PermittedProcess::MasterDnsVpn lookup resolves to a stable name
        # across version bumps.
        for fname in os.listdir(self.build_folder):
            if not fname.startswith("MasterDnsVPN_Client_"):
                continue
            full = os.path.join(self.build_folder, fname)
            if not os.path.isfile(full):
                continue
            if not (os.access(full, os.X_OK) or fname.endswith(".exe")):
                continue
            target = "mdnsvpn.exe" if self._is_windows else "mdnsvpn"
            copy(self, fname, src=self.build_folder, dst=self.package_folder)
            os.rename(
                os.path.join(self.package_folder, fname),
                os.path.join(self.package_folder, target),
            )
            break
        else:
            raise RuntimeError(
                "amnezia-mdnsvpn: no MasterDnsVPN_Client_* binary found in "
                "the unpacked release archive — recipe assumed the wrong "
                "asset layout."
            )

    def package_info(self):
        self.cpp_info.exe = True
        self.cpp_info.location = os.path.join(self.package_folder, f"mdnsvpn{self._ext}")
        self.cpp_info.set_property("cmake_target_name", "masterking32::mdnsvpn")
