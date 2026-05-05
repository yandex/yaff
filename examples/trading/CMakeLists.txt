set(_generated_dir ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${_generated_dir})

add_executable(yaff_example_trading
    main.cpp
    proto/order.proto
)

protobuf_generate(
    TARGET yaff_example_trading
    LANGUAGE cpp
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    PROTOC_OUT_DIR ${_generated_dir}
)

yaff_generate(
    TARGET yaff_example_trading
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    OUT_DIR ${_generated_dir}
)

target_include_directories(yaff_example_trading PRIVATE ${_generated_dir})

target_link_libraries(yaff_example_trading
    PRIVATE
        yaff::core
        yaff::proto
)
