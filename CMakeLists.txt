cmake_minimum_required(VERSION 3.20)

project(yaff
    VERSION 0.1.0
    DESCRIPTION "Yet another Flat Format"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(YAFF_BUILD_TESTS "Build YaFF tests" OFF)
option(YAFF_BUILD_BENCHMARKS "Build YaFF benchmarks" OFF)
option(YAFF_BUILD_EXAMPLES "Build YaFF examples" OFF)

find_package(Protobuf CONFIG REQUIRED)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(YAFF_PROTO_IMPORT_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

add_subdirectory(include)
add_subdirectory(src/compilation)
add_subdirectory(src/protoc-plugin)

if(YAFF_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(YAFF_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(YAFF_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

install(FILES
    cmake/YaFFProtocPluginTarget.cmake
    cmake/YaFFGenerate.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/YaFF
)

install(EXPORT YaFFTargets
    NAMESPACE yaff::
    FILE YaFFTargets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/YaFF
)

configure_package_config_file(
    cmake/YaFFConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/YaFFConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/YaFF
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/YaFFConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/YaFFConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/YaFFConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/YaFF
)
