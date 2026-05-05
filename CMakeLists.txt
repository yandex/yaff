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

option(YAFF_BUILD_TESTS "Build YaFF tests" ON)
option(YAFF_BUILD_BENCHMARKS "Build YaFF benchmarks" OFF)
option(YAFF_BUILD_EXAMPLES "Build YaFF examples" OFF)

find_package(Protobuf CONFIG REQUIRED)

set(YAFF_PROTO_IMPORT_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

add_subdirectory(include)
add_subdirectory(src/compilation)
add_subdirectory(src/protoc-plugin)

if(YAFF_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

if(YAFF_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(YAFF_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
