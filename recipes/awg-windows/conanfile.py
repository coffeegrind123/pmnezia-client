from conan import ConanFile
from conan.tools.layout import basic_layout
from conan.errors import ConanInvalidConfiguration
from conan.tools.files import get, copy, chdir, replace_in_file
from conan.tools.gnu import AutotoolsToolchain

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


class AwgWindows(ConanFile):
    name = "awg-windows"
    version = "3.1.20260814"
    settings = "os", "arch"

    @property
    def _goarm(self):
        return {
            "armv5el": "5",
            "armv5hf": "5",
            "armv6": "6",
            "armv7": "7",
            "armv7hf": "7",
            "armv7s": "7",
            "armv7k": "7",
        }.get(str(self.settings.arch))
    
    @property
    def _goarch(self):
        return {
            "x86": "386",
            "x86_64": "amd64",
            "armv5el": "arm",
            "armv5hf": "arm",
            "armv6": "arm",
            "armv7": "arm",
            "armv7hf": "arm",
            "armv7s": "arm",
            "armv7k": "arm",
            "armv8": "arm64",
            "armv8_32": "arm64",
            "armv8.3": "arm64",
            "arm64ec": "arm64"
        }.get(str(self.settings.arch))

    def layout(self):
        basic_layout(self)

    def validate(self):
        if not str(self.settings.os).startswith("Windows"):
            raise ConanInvalidConfiguration(
                f"{self.name} v{self.version} is to be used on Windows only!"
            )
        if not self._goarch:
            raise ConanInvalidConfiguration(
                f"{self.name} v{self.version} does not support {self.settings.arch} architecture"
            )

    def build_requirements(self):
        self.tool_requires("mingw-builds/15.1.0")
        self.tool_requires("go/1.26.0")

    def requirements(self):
        self.requires("wintun/[*]")

    def source(self):
        get(self, f"https://github.com/amnezia-vpn/amneziawg-windows/archive/refs/tags/v{self.version}.zip",
            sha256="d941861e3c0fada70b6b66b08aad4c77098d612aa11dd41b8ad70dd8afa6c61b", strip_root=True)
        pin_awg_go(self, self.source_folder)
        
    def generate(self):
        tc = AutotoolsToolchain(self)
        tc.extra_cflags = [
            "-Wall",
            "-Wno-unused-function",
            "-Wno-switch",
            "-DWINVER=0x0601"
        ]
        tc.extra_ldflags = [ 
            "-Wl,--dynamicbase",
            "-Wl,--nxcompat",
            "-Wl,--export-all-symbols",
            "-Wl,--high-entropy-va"
        ]
        env = tc.environment()
        env.define("GOOS", "windows")
        if self._goarm:
            env.define("GOARM", self._goarm)
        env.define("GOARCH", self._goarch)
        env.define("CGO_ENABLED", "1")
        env.define("CGO_LDFLAGS", tc.ldflags)
        env.define("CGO_CFLAGS", tc.cflags)
        tc.generate(env)

    def build(self):
        with chdir(self, self.source_folder):
            self.run(f'go build -buildmode c-shared -ldflags="-w -s" -trimpath -v -o "{os.path.join(self.build_folder, "tunnel.dll")}"')

    def package(self):
        copy(self, "tunnel.dll", src=self.build_folder, dst=os.path.join(self.package_folder, "bin"))

    def package_info(self):
        self.cpp_info.exe = True
        self.cpp_info.location = os.path.join(self.package_folder, "bin", "tunnel.dll")
        self.cpp_info.set_property("cmake_target_name", "amnezia::awg-windows")
