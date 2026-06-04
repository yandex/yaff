import os
from conan import ConanFile
from conan.tools.files import load
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout


class YaffConan(ConanFile):
    name = "yaff"
    description = "Yet Another Flat Format"
    license = "Apache-2.0"

    exports_sources = (
        "CMakeLists.txt",
        "VERSION",
        "cmake/*",
        "include/*",
        "src/*",
        "tests/*",
        "benchmarks/*",
        "examples/*",
    )

    settings = "os", "compiler", "build_type", "arch"
    options = {
        "build_tests":      [True, False],
        "build_benchmarks": [True, False],
        "build_examples":   [True, False],
    }
    default_options = {
        "build_tests":      False,
        "build_benchmarks": False,
        "build_examples":   False,
    }

    def set_version(self):
        self.version = load(self, os.path.join(self.recipe_folder, "VERSION")).strip()

    def requirements(self):
        self.requires("protobuf/[>=5.26]")

        if self.options.build_tests:
            self.requires("gtest/[>=1.13]")

        if self.options.build_benchmarks:
            self.requires("benchmark/[>=1.8.2]")
            self.requires("flatbuffers/[>=23.5.9]")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["YAFF_BUILD_TESTS"]      = self.options.build_tests
        tc.variables["YAFF_BUILD_BENCHMARKS"] = self.options.build_benchmarks
        tc.variables["YAFF_BUILD_EXAMPLES"]   = self.options.build_examples
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property(
            "cmake_build_modules",
            [
                os.path.join("lib", "cmake", "YaFF", "YaFFProtocPluginTarget.cmake"),
                os.path.join("lib", "cmake", "YaFF", "YaFFGenerate.cmake"),
            ]
        )

        self.cpp_info.components["core"].set_property("cmake_target_name", "yaff::core")
        self.cpp_info.components["core"].includedirs = ["include"]
        self.cpp_info.components["core"].libdirs = []
        self.cpp_info.components["core"].bindirs = []

        self.cpp_info.components["proto"].set_property("cmake_target_name", "yaff::proto")
        self.cpp_info.components["proto"].libs = ["yaff_proto"]
        self.cpp_info.components["proto"].requires = ["core", "protobuf::libprotobuf"]

