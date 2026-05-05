add_library(yaff_core INTERFACE)
add_library(yaff::core ALIAS yaff_core)

target_include_directories(yaff_core
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

add_subdirectory(yaff/proto)
