set(_generated_dir ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${_generated_dir})

add_executable(yaff_example_feed_api
    main.cpp
    proto/feed.proto
)

protobuf_generate(
    TARGET yaff_example_feed_api
    LANGUAGE cpp
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    PROTOC_OUT_DIR ${_generated_dir}
)

yaff_generate(
    TARGET yaff_example_feed_api
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    OUT_DIR ${_generated_dir}
)

target_include_directories(yaff_example_feed_api PRIVATE ${_generated_dir})

target_link_libraries(yaff_example_feed_api
    PRIVATE
        yaff::core
        yaff::proto
)
