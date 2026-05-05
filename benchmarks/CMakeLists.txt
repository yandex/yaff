find_package(benchmark CONFIG REQUIRED)
find_package(flatbuffers CONFIG REQUIRED)

include(${PROJECT_SOURCE_DIR}/cmake/yaff-generate.cmake)
include(BuildFlatBuffers)

add_subdirectory(access)
