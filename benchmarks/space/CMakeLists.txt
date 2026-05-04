set(YAFF_BENCH_SPACE_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${YAFF_BENCH_SPACE_GENERATED_DIR})
file(MAKE_DIRECTORY ${YAFF_BENCH_SPACE_GENERATED_DIR}/protoyaff)

add_library(yaff_bench_space_protoyaff STATIC
    protoyaff/flat.proto
)

protobuf_generate(
    TARGET yaff_bench_space_protoyaff
    LANGUAGE cpp
    IMPORT_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${YAFF_PROTO_IMPORT_DIR}
    PROTOC_OUT_DIR ${YAFF_BENCH_SPACE_GENERATED_DIR}
)

yaff_generate(
    TARGET yaff_bench_space_protoyaff
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    OUT_DIR ${YAFF_BENCH_SPACE_GENERATED_DIR}
)

target_include_directories(yaff_bench_space_protoyaff PUBLIC ${YAFF_BENCH_SPACE_GENERATED_DIR})
target_link_libraries(yaff_bench_space_protoyaff PUBLIC yaff::core yaff::proto)

flatbuffers_generate_headers(
    TARGET yaff_bench_space_flatbuffers
    INCLUDE_PREFIX flatbuffers
    SCHEMAS
        ${CMAKE_CURRENT_SOURCE_DIR}/flatbuffers/flat.fbs
)

target_link_libraries(yaff_bench_space_flatbuffers INTERFACE flatbuffers::flatbuffers)

function(yaff_add_space_benchmark _src)
    get_filename_component(_name "${_src}" NAME_WE)
    set(_target "yaff_bench_space_${_name}")
    add_executable(${_target} ${_src})
    target_link_libraries(${_target}
        PRIVATE
            yaff_bench_space_protoyaff
            yaff_bench_space_flatbuffers
            benchmark::benchmark_main
    )
    add_dependencies(${_target} GENERATE_yaff_bench_space_flatbuffers)
endfunction()

yaff_add_space_benchmark(flat.cpp)
