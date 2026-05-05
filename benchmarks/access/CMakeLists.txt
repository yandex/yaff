set(YAFF_BENCH_ACCESS_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${YAFF_BENCH_ACCESS_GENERATED_DIR})
file(MAKE_DIRECTORY ${YAFF_BENCH_ACCESS_GENERATED_DIR}/protoyaff)

add_library(yaff_bench_access_protoyaff STATIC
    protoyaff/flat.proto
    protoyaff/hierarchical.proto
)

protobuf_generate(
    TARGET yaff_bench_access_protoyaff
    LANGUAGE cpp
    IMPORT_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${YAFF_PROTO_IMPORT_DIR}
    PROTOC_OUT_DIR ${YAFF_BENCH_ACCESS_GENERATED_DIR}
)

yaff_generate(
    TARGET yaff_bench_access_protoyaff
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    OUT_DIR ${YAFF_BENCH_ACCESS_GENERATED_DIR}
)

target_include_directories(yaff_bench_access_protoyaff PUBLIC ${YAFF_BENCH_ACCESS_GENERATED_DIR})
target_link_libraries(yaff_bench_access_protoyaff PUBLIC yaff::core yaff::proto)

if(MSVC)
    target_compile_options(yaff_bench_access_protoyaff PRIVATE /O2 /DNDEBUG)
else()
    target_compile_options(yaff_bench_access_protoyaff PRIVATE -O3 -DNDEBUG)
endif()

flatbuffers_generate_headers(
    TARGET yaff_bench_access_flatbuffers
    INCLUDE_PREFIX flatbuffers
    SCHEMAS
        ${CMAKE_CURRENT_SOURCE_DIR}/flatbuffers/flat.fbs
        ${CMAKE_CURRENT_SOURCE_DIR}/flatbuffers/hierarchical.fbs
)

target_link_libraries(yaff_bench_access_flatbuffers INTERFACE flatbuffers::flatbuffers)

function(yaff_add_access_benchmark _src)
    get_filename_component(_name "${_src}" NAME_WE)
    set(_target "yaff_bench_access_${_name}")
    add_executable(${_target} ${_src})
    target_link_libraries(${_target}
        PRIVATE
            yaff_bench_access_protoyaff
            yaff_bench_access_flatbuffers
            benchmark::benchmark_main
    )
    add_dependencies(${_target} GENERATE_yaff_bench_access_flatbuffers)
    if(MSVC)
        target_compile_options(${_target} PRIVATE /O2 /DNDEBUG)
    else()
        target_compile_options(${_target} PRIVATE -O3 -DNDEBUG)
    endif()
endfunction()

yaff_add_access_benchmark(flat.cpp)
yaff_add_access_benchmark(hierarchical.cpp)
