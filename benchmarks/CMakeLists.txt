find_package(benchmark CONFIG REQUIRED)
find_package(flatbuffers CONFIG REQUIRED)

include(${PROJECT_SOURCE_DIR}/cmake/YaFFGenerate.cmake)

add_subdirectory(access)
add_subdirectory(space)
