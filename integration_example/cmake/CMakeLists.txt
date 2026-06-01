cmake_minimum_required(VERSION 3.20)
project(yaff_smoke_cmake LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(YaFF CONFIG REQUIRED)

set(_generated_dir ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${_generated_dir})

add_executable(yaff_smoke
    ../src/main.cpp
    ../proto/smoke.proto
)

protobuf_generate(
    TARGET yaff_smoke
    LANGUAGE cpp
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/..
    PROTOC_OUT_DIR ${_generated_dir}
)

yaff_generate(
    TARGET yaff_smoke
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/..
    OUT_DIR ${_generated_dir}
)

target_include_directories(yaff_smoke PRIVATE ${_generated_dir})

target_link_libraries(yaff_smoke
    PRIVATE
        yaff::core
        yaff::proto
)
