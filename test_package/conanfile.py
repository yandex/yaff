import os
from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.build import can_run


class YaffSmokeConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    test_type = "explicit"

    def requirements(self):
        # Emulates a consumer project that pins its own protobuf version.
        self.requires("protobuf/5.27.0")

        self.requires(self.tested_reference_str)

    def build_requirements(self):
        self.tool_requires("protobuf/<host_version>")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if can_run(self):
            bin_path = os.path.join(self.cpp.build.bindir, "yaff_smoke")
            self.run(bin_path, env="conanrun")
