set(YAFF_COMPILATION_SOURCES
    compiler.cpp
    cpp_gen.cpp
    error.cpp
    ir.cpp
    ir_compat.cpp
    ir_processor.cpp
    ir_reflect.cpp
    proto_front.cpp
    util.cpp
)

add_library(yaff_compilation STATIC ${YAFF_COMPILATION_SOURCES})
add_library(yaff::compilation ALIAS yaff_compilation)

target_link_libraries(yaff_compilation
    PUBLIC
        yaff::core
        yaff::proto
)

target_include_directories(yaff_compilation
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
)

if(MSVC)
    target_compile_options(yaff_compilation PRIVATE /W4)
else()
    target_compile_options(yaff_compilation PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS yaff_compilation
    EXPORT YaffTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
