add_library(yaff_core INTERFACE)
add_library(yaff::core ALIAS yaff_core)

set_target_properties(yaff_core PROPERTIES EXPORT_NAME core)

target_compile_features(yaff_core INTERFACE cxx_std_20)

target_include_directories(yaff_core
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

install(TARGETS yaff_core
    EXPORT YaFFTargets
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

add_subdirectory(yaff/proto)
