find_package(benchmark CONFIG REQUIRED)
find_package(flatbuffers CONFIG REQUIRED)

include(${PROJECT_SOURCE_DIR}/cmake/yaff-generate.cmake)
include(${flatbuffers_DIR}/BuildFlatBuffers.cmake)

add_subdirectory(access)
add_subdirectory(space)
