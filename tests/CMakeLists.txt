find_package(GTest CONFIG REQUIRED)

include(${PROJECT_SOURCE_DIR}/cmake/YaFFGenerate.cmake)

set(YAFF_TEST_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${YAFF_TEST_GENERATED_DIR})

add_library(yaff_test_protoyaff STATIC
    protoyaff/proto_api.proto
    protoyaff/proto2.proto
    protoyaff/proto3.proto
)

protobuf_generate(
    TARGET yaff_test_protoyaff
    LANGUAGE cpp
    IMPORT_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${YAFF_PROTO_IMPORT_DIR}
    PROTOC_OUT_DIR ${YAFF_TEST_GENERATED_DIR}
)

yaff_generate(
    TARGET yaff_test_protoyaff
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    OUT_DIR ${YAFF_TEST_GENERATED_DIR}
)

target_include_directories(yaff_test_protoyaff PUBLIC ${YAFF_TEST_GENERATED_DIR})
target_link_libraries(yaff_test_protoyaff PUBLIC yaff::core yaff::proto)

function(yaff_add_unit_test _src)
    get_filename_component(_name "${_src}" NAME_WE)
    set(_target "yaff_${_name}_test")
    add_executable(${_target} ${_src})
    target_link_libraries(${_target}
        PRIVATE
            yaff_test_protoyaff
            GTest::gtest_main
    )
    add_test(NAME ${_target} COMMAND ${_target})
    yaff_apply_sanitizers(${_target})
endfunction()

yaff_add_unit_test(common.cpp)
yaff_add_unit_test(proto_api.cpp)
yaff_add_unit_test(fixed_message.cpp)
yaff_add_unit_test(flat_message.cpp)
yaff_add_unit_test(sparse_message.cpp)
