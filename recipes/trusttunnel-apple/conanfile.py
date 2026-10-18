from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.internal.model.pkg_type import PackageType
from conan.tools.files import download, copy
from conan.tools.layout import basic_layout

import os
import shutil


required_conan_version = ">=2.26"


class TrustTunnelApple(ConanFile):
    """TrustTunnelClient's VpnClientFramework.xcframework for the iOS and
    macOS network extensions, which cannot exec trusttunnel_client.

    Built from upstream source by .github/workflows/trusttunnel-apple.yml and
    published as a release asset of this repo, because AdGuard ships the same
    framework only through GitHub Packages (token required). The zip is pinned
    by sha256 here; a rebuild never changes what the app links until this
    file is bumped.

    The framework is dynamic, linked against system frameworks only, with
    BoringSSL/libevent/nghttp2 inside and unexported, so it does not meet the
    app's OpenSSL. It must be embedded in, and signed with, the extension.
    """

    name = "trusttunnel-apple"
    version = "1.1.11"
    package_type = "shared-library"
    settings = "os"

    _release = "https://github.com/coffeegrind123/pmnezia-client/releases/download"
    _sha256 = {
        "1.1.11": "52e43266ef6d1260615ca1e7d94830c9b6e1d3d2edb3c91eb692de9b0879ced7",
    }

    def layout(self):
        basic_layout(self)

    def validate(self):
        if str(self.settings.os) not in ("iOS", "Macos"):
            raise ConanInvalidConfiguration(f"{self.name} is for iOS and macOS only")

    def source(self):
        pass

    def build(self):
        archive = os.path.join(self.build_folder, "framework.zip")
        download(self,
                 f"{self._release}/trusttunnel-apple-v{self.version}/VpnClientFramework-{self.version}.xcframework.zip",
                 archive, sha256=self._sha256[self.version])
        # Not conan's get(): Python's zipfile writes symlinks out as small text
        # files, and the macOS slice is a versioned framework whose Headers,
        # Modules and binary are symlinks into Versions/A. ditto restores them.
        self.run(f'ditto -x -k "{archive}" "{self.build_folder}"')
        os.remove(archive)

    @property
    def _root(self):
        return os.path.join(self.build_folder, f"VpnClientFramework-{self.version}")

    def package(self):
        shutil.copytree(src=os.path.join(self._root, "VpnClientFramework.xcframework"),
                        dst=os.path.join(self.package_folder, "VpnClientFramework.xcframework"),
                        symlinks=True)
        headers = os.path.join(self.package_folder, "VpnClientFramework.xcframework", "macos-arm64_x86_64",
                               "VpnClientFramework.framework", "Headers")
        if not os.path.islink(headers):
            raise RuntimeError(f"{headers} is not a symlink; the framework was unpacked without its symlinks")
        copy(self, "LICENSE", src=self._root, dst=os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        xcframework = os.path.join(self.package_folder, "VpnClientFramework.xcframework")
        self.cpp_info.set_property("cmake_target_name", "trusttunnel::vpnclient-framework")
        self.cpp_info.type = PackageType.SHARED
        self.cpp_info.package_framework = True
        self.cpp_info.location = xcframework
        # Embedding needs the path itself (XCODE_EMBED_FRAMEWORKS).
        self.cpp_info.set_property("cmake_extra_variables", {
            "TRUSTTUNNEL_APPLE_XCFRAMEWORK": xcframework.replace("\\", "/")
        })
