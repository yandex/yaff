add_executable(yaff_protoc_plugin
    main.cpp
    protoc_plugin.cpp
)
add_executable(yaff::protoc_plugin ALIAS yaff_protoc_plugin)

set_target_properties(yaff_protoc_plugin PROPERTIES
    OUTPUT_NAME protoc-gen-yaff
)

target_link_libraries(yaff_protoc_plugin
    PRIVATE
        yaff::compilation
        protobuf::libprotoc
)

if(MSVC)
    target_compile_options(yaff_protoc_plugin PRIVATE /W4)
else()
    target_compile_options(yaff_protoc_plugin PRIVATE -Wall -Wextra -Wpedantic)
endif()
