from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.layout import basic_layout
from conan.tools.files import get, copy, collect_libs, replace_in_file
from conan.tools.apple import is_apple_os
from conan.tools.gnu import AutotoolsToolchain, Autotools

import os

# The newest amneziawg-{android,windows,apple} tags still embed amneziawg-go
# v3.1.20260814. v3.1.20260828 keeps ContentPaddingAddition padding inside the
# UDP window (it could overrun the MTU) and turns all underload handling off
# with DisableCookies, matching the 3.1.20260906 kernel module servers run.
# Repin the module until a platform tag ships it. The go.sum hash is
# sum.golang.org's for that tag, so `go build` still verifies the download;
# the go.mod hash is unchanged because the dependency set is. replace_in_file
# fails the build if upstream's lines stop matching. The same block is in
# recipes/awg-android, awg-windows and awg-apple; keep them and
# recipes/awg-go on one version.
AWG_GO_MODULE = "github.com/amnezia-vpn/amneziawg-go/v3"
AWG_GO_EMBEDDED = "v3.1.20260814"
AWG_GO_EMBEDDED_SUM = "h1:l2AhBD+sFycU8Im81n/bZORMxW7fWtlZJEuJ4Hh0+z0="
AWG_GO_PINNED = "v3.1.20260828"
AWG_GO_PINNED_SUM = "h1:D8d8gGvwXcTxUIsE4z6F6vjy4/VZddu95vMNtOygh1c="


def pin_awg_go(conanfile, module_dir):
    replace_in_file(conanfile, os.path.join(module_dir, "go.mod"),
                    f"{AWG_GO_MODULE} {AWG_GO_EMBEDDED}",
                    f"{AWG_GO_MODULE} {AWG_GO_PINNED}")
    go_sum = os.path.join(module_dir, "go.sum")
    replace_in_file(conanfile, go_sum,
                    f"{AWG_GO_MODULE} {AWG_GO_EMBEDDED} {AWG_GO_EMBEDDED_SUM}",
                    f"{AWG_GO_MODULE} {AWG_GO_PINNED} {AWG_GO_PINNED_SUM}")
    replace_in_file(conanfile, go_sum,
                    f"{AWG_GO_MODULE} {AWG_GO_EMBEDDED}/go.mod ",
                    f"{AWG_GO_MODULE} {AWG_GO_PINNED}/go.mod ")


class AwgApple(ConanFile):
    name = "awg-apple"
    version = "3.1.4"
    settings = "os", "arch", "compiler"

    @property
    def _goarch(self):
        arch_map = {
            "armv8": "arm64",
            "x86_64": "x86_64",
        }
        archs = str(self.settings.arch).split("|")
        return " ".join(arch_map.get(arch, arch) for arch in archs)

    def configure(self):
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def layout(self):
        basic_layout(self, build_folder=os.path.join(self.folders.source, "Sources/WireGuardKitGo"))

    def build_requirements(self):
        self.tool_requires("go/1.26.0")

    def validate(self):
        if not is_apple_os(self):
            raise ConanInvalidConfiguration(
                f"{self.name} v{self.version} does not support {self.settings.os}"
            )

    def source(self):
        get(self, f"https://github.com/amnezia-vpn/amneziawg-apple/archive/refs/tags/v{self.version}.zip",
            sha256="09d7b760d18232fdf121ed2286b2f171b501dc31137e5e7d557c1ee3a99ef772", strip_root=True
        )
        pin_awg_go(self, os.path.join(self.source_folder, "Sources", "WireGuardKitGo"))

    def generate(self):
        tc = AutotoolsToolchain(self)
        sdk = self.settings.get_safe("os.sdk", "macosx")
        tc.make_args = [
            f"ARCHS={self._goarch}",
            f"PLATFORM_NAME={sdk}"
        ]
        tc.generate()

    def build(self):
        autotools = Autotools(self)
        autotools.make()
        autotools.make("version-header")

    def package(self):
        copy(self, "wireguard.h", src=self.build_folder, dst=os.path.join(self.package_folder, "include"))
        copy(self, "*.h", src=os.path.join(self.build_folder, "out"), dst=os.path.join(self.package_folder, "include"))
        copy(self, "*.a", src=os.path.join(self.build_folder, "out"), dst=os.path.join(self.package_folder, "lib"))

    def package_info(self):
        self.cpp_info.set_property("cmake_target_name", "amnezia::awg-apple")
        self.cpp_info.libs = collect_libs(self)
