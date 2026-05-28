set(_generated_dir ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${_generated_dir})

add_executable(yaff_example_doc_views
    main.cpp
    proto/document.proto
)

protobuf_generate(
    TARGET yaff_example_doc_views
    LANGUAGE cpp
    IMPORT_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${YAFF_PROTO_IMPORT_DIR}
    PROTOC_OUT_DIR ${_generated_dir}
)

# The document.proto declares two slices, "light_view" and "full_view"; each
# one is generated into its own namespace so they can coexist in a single
# translation unit.
yaff_generate(
    TARGET yaff_example_doc_views
    TAG light_view
    NAMESPACE light_view
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    OUT_DIR ${_generated_dir}
)

yaff_generate(
    TARGET yaff_example_doc_views
    TAG full_view
    NAMESPACE full_view
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    OUT_DIR ${_generated_dir}
)

target_include_directories(yaff_example_doc_views PRIVATE ${_generated_dir})

target_link_libraries(yaff_example_doc_views
    PRIVATE
        yaff::core
        yaff::proto
)
