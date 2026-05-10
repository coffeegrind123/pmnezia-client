from conan import ConanFile
from conan.tools.files import copy
from conan.tools.layout import basic_layout
from conan.errors import ConanInvalidConfiguration
from conan.tools.env import VirtualBuildEnv, Environment

import os
import subprocess

from pathlib import Path

# Builds the gomobile-bound MasterDnsVPN AAR consumed by the
# :master_dns_vpn:libMasterDnsVpn Gradle subproject.
#
# The Go source lives in-tree at
#   client/android/master_dns_vpn/libMasterDnsVpn/gomobile/
# (alongside the README that documents the AAR contract). At build time the
# recipe drives `gomobile bind` over that source, producing libmasterdnsvpn.aar
# and dropping it into the same directory so Gradle's flat-file artifact
# wrapper picks it up.
#
# Mirror of recipes/amnezia-libxray for the structurally identical xray
# library — see that recipe for the same shape applied to a different upstream.


class AmneziaMdnsvpnAndroid(ConanFile):
    name = "amnezia-mdnsvpn-android"
    version = "2026.05.10"
    settings = "os", "arch", "compiler"

    def configure(self):
        # Android NDK builds don't carry a libcxx setting we care about — drop
        # the constraints so the recipe can be reused across NDK versions.
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def layout(self):
        basic_layout(self, build_folder=".")

    def build_requirements(self):
        # Same Go toolchain version libxray pins — keeps the gomobile bind
        # output reproducible across recipes.
        self.tool_requires("go/1.26.0")

    def validate(self):
        if self.settings.os != "Android":
            raise ConanInvalidConfiguration(
                f"{self.name} v{self.version} is Android-only; "
                f"the desktop binary lives in `amnezia-mdnsvpn`."
            )

    def generate(self):
        VirtualBuildEnv(self).generate()
        env = Environment()
        ndk_path_str = self.conf.get("tools.android:ndk_path")
        if ndk_path_str:
            ndk_path = Path(ndk_path_str)
            if len(ndk_path.parts) > 2:
                sdk_path = ndk_path.parents[1]
                env.define("ANDROID_HOME", str(sdk_path))
        env.vars(self).save_script("conan_provide_androidhome")

    def _gomobile_source_dir(self):
        # The gomobile wrapper Go source ships in the client tree so reviewers
        # can audit the JVM-callable surface without checking out the recipe
        # cache. recipe_folder = .../recipes/amnezia-mdnsvpn-android/.
        return Path(self.recipe_folder).parents[1] / (
            "client/android/master_dns_vpn/libMasterDnsVpn/gomobile"
        )

    def build(self):
        gomobile_dir = self._gomobile_source_dir()
        if not gomobile_dir.is_dir():
            raise RuntimeError(
                f"amnezia-mdnsvpn-android: gomobile source dir not found at "
                f"{gomobile_dir} — recipe expected to be built from the "
                f"amnezia-client tree, not in isolation."
            )

        # Install + initialise gomobile inside the conan-managed Go cache.
        self.run("go install golang.org/x/mobile/cmd/gomobile@latest", cwd=str(gomobile_dir))
        self.run("go install golang.org/x/mobile/cmd/gobind@latest", cwd=str(gomobile_dir))
        self.run("gomobile init", cwd=str(gomobile_dir))

        # Resolve the upstream MasterDnsVPN Go module to its release tag. The
        # tag lives in-tree to keep the recipe + go.mod in sync.
        self.run(
            "go mod edit -replace "
            "github.com/masterking32/MasterDnsVPN=github.com/masterking32/MasterDnsVPN@v0.0.0-20260510180256-27c7e1100000",
            cwd=str(gomobile_dir),
        )
        self.run("go mod tidy", cwd=str(gomobile_dir))

        out = os.path.join(self.build_folder, "libmasterdnsvpn.aar")
        self.run(
            "gomobile bind "
            "-target=android/arm64,arm,amd64,386 "
            "-androidapi 21 "
            "-trimpath "
            "-ldflags='-s -w' "
            f"-o {out} .",
            cwd=str(gomobile_dir),
        )

    def package(self):
        copy(
            self,
            "libmasterdnsvpn.aar",
            src=self.build_folder,
            dst=os.path.join(self.package_folder, "aar"),
        )

    def package_info(self):
        self.cpp_info.set_property(
            "cmake_extra_variables",
            {
                "AMNEZIA_MDNSVPN_ANDROID_AAR_PATH": os.path.join(
                    self.package_folder, "aar", "libmasterdnsvpn.aar"
                ),
            },
        )
