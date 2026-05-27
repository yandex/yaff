set(YAFF_PROTO_OPTIONS ${CMAKE_CURRENT_SOURCE_DIR}/options.proto)
set(YAFF_PROTO_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${YAFF_PROTO_GENERATED_DIR})

protobuf_generate(
    OUT_VAR YAFF_PROTO_GENERATED_FILES
    LANGUAGE cpp
    PROTOS ${YAFF_PROTO_OPTIONS}
    IMPORT_DIRS ${YAFF_PROTO_IMPORT_DIR}
    PROTOC_OUT_DIR ${YAFF_PROTO_GENERATED_DIR}
)

add_library(yaff_proto STATIC ${YAFF_PROTO_GENERATED_FILES})
add_library(yaff::proto ALIAS yaff_proto)

set_target_properties(yaff_proto PROPERTIES EXPORT_NAME proto)

target_link_libraries(yaff_proto PUBLIC protobuf::libprotobuf)

target_include_directories(yaff_proto
    PUBLIC
        $<BUILD_INTERFACE:${YAFF_PROTO_IMPORT_DIR}>
        $<BUILD_INTERFACE:${YAFF_PROTO_GENERATED_DIR}>
        $<INSTALL_INTERFACE:include>
)

install(TARGETS yaff_proto
    EXPORT YaffTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/options.proto"
    "${YAFF_PROTO_GENERATED_DIR}/yaff/proto/options.pb.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/yaff/proto
)
