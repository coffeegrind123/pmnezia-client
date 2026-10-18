from conan import ConanFile
from conan.tools.layout import basic_layout
from conan.tools.files import download, copy

import os

class V2rayRulesDat(ConanFile):
    name = "v2ray-rules-dat"
    version = "202610032254"

    def layout(self):
        basic_layout(self, build_folder=".")

    def source(self):
        # TODO(ygurov): build from source instead of plain copying
        download(self, filename="geoip.dat", url=f"https://github.com/Loyalsoldier/v2ray-rules-dat/releases/download/{self.version}/geoip.dat",
            sha256="391b522361c52804e486a98b53d97f3d9c1d3e4e4217bb34954774a0b456b9fd")
        download(self, filename="geosite.dat", url=f"https://github.com/Loyalsoldier/v2ray-rules-dat/releases/download/{self.version}/geosite.dat",
            sha256="59b6ed33176b6f03debcfa3105d42de268475ffa8a1e010f481985256e375262")

    def package(self):
        copy(self, "*.dat", src=self.build_folder, dst=self.package_folder)

    def package_info(self):
        self.cpp_info.set_property("cmake_target_name", "Loyalsoldier::v2ray-rules-dat")
        self.cpp_info.set_property("cmake_extra_variables", {
            "GEOSITE_DAT_PATH": os.path.join(self.package_folder, "geosite.dat").replace("\\", "/"),
            "GEOIP_DAT_PATH": os.path.join(self.package_folder, "geoip.dat").replace("\\", "/")
        })
