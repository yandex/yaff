math(EXPR YAFF_VERSION_NUMBER
    "${PROJECT_VERSION_MAJOR} * 10000 + ${PROJECT_VERSION_MINOR} * 100 + ${PROJECT_VERSION_PATCH}"
)

configure_file(
    yaff/version.h.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/yaff/version.h"
    @ONLY
)

add_library(yaff_core INTERFACE)
add_library(yaff::core ALIAS yaff_core)

set_target_properties(yaff_core PROPERTIES EXPORT_NAME core)

target_compile_features(yaff_core INTERFACE cxx_std_20)

target_include_directories(yaff_core
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:include>
)

install(TARGETS yaff_core
    EXPORT YaFFTargets
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/generated/yaff/version.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/yaff
)

add_subdirectory(yaff/proto)
