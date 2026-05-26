add_library(yaff_core INTERFACE)
add_library(yaff::core ALIAS yaff_core)

target_include_directories(yaff_core
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

install(TARGETS yaff_core
    EXPORT YaffTargets
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

add_subdirectory(yaff/proto)
