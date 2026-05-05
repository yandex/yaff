function(yaff_generate)
    set(_singleargs TARGET OUT_VAR OUT_DIR TAG NAMESPACE)
    set(_multiargs PROTOS IMPORT_DIRS)

    cmake_parse_arguments(YAFF "" "${_singleargs}" "${_multiargs}" "${ARGN}")

    # Optional plugin tag (slice). When set, generated files are named
    # <name>_<tag>.yaff.h/.cpp, matching the protoc plugin behaviour.
    if(YAFF_TAG)
        set(_yaff_suffix "_${YAFF_TAG}")
    else()
        set(_yaff_suffix "")
    endif()

    # Build the plugin parameter string ("k1=v1,k2=v2") that protoc passes
    # through to protoc-gen-yaff via the option syntax.
    set(_plugin_options "")
    if(YAFF_TAG)
        list(APPEND _plugin_options "tag=${YAFF_TAG}")
    endif()
    if(YAFF_NAMESPACE)
        list(APPEND _plugin_options "namespace=${YAFF_NAMESPACE}")
    endif()
    list(JOIN _plugin_options "," _plugin_options_str)

    if(YAFF_TARGET)
        get_target_property(_target_dir ${YAFF_TARGET} SOURCE_DIR)
        get_target_property(_target_sources ${YAFF_TARGET} SOURCES)
        foreach(_src ${_target_sources})
            if(_src MATCHES "\\.proto$")
                get_filename_component(_abs_src "${_src}" ABSOLUTE BASE_DIR "${_target_dir}")
                list(APPEND YAFF_PROTOS ${_abs_src})
            endif()
        endforeach()
    endif()

    if(NOT YAFF_PROTOS)
        message(SEND_ERROR "Error: yaff_generate was called without target and source files")
        return()
    endif()

    if(NOT YAFF_OUT_DIR)
        set(YAFF_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    endif()

    if(TARGET yaff::protoc_plugin)
        set(_plugin_bin "$<TARGET_FILE:yaff::protoc_plugin>")
        set(_plugin_dep yaff::protoc_plugin)
    else()
        find_program(_yaff_generate_plugin_bin protoc-gen-yaff REQUIRED)
        set(_plugin_bin "${_yaff_generate_plugin_bin}")
        set(_plugin_dep "")
        unset(_yaff_generate_plugin_bin CACHE)
    endif()

    set(_import_roots "${CMAKE_CURRENT_SOURCE_DIR}" ${YAFF_IMPORT_DIRS} "${YAFF_PROTO_IMPORT_DIR}")
    set(_import_flags "")
    foreach(_dir ${_import_roots})
        get_filename_component(_abs_dir "${_dir}" ABSOLUTE)
        list(APPEND _import_flags "-I${_abs_dir}")
    endforeach()

    set(_generated_files "")
    foreach(_proto ${YAFF_PROTOS})
        get_filename_component(_abs_proto "${_proto}" ABSOLUTE)
        get_filename_component(_name "${_proto}" NAME_WE)

        set(_rel_dir "")
        foreach(_root ${_import_roots})
            get_filename_component(_abs_root "${_root}" ABSOLUTE)
            file(RELATIVE_PATH _candidate_rel "${_abs_root}" "${_abs_proto}")
            if(NOT _candidate_rel MATCHES "^\\.\\.")
                get_filename_component(_rel_dir "${_candidate_rel}" DIRECTORY)
                break()
            endif()
        endforeach()

        if(_rel_dir)
            set(_out_dir "${YAFF_OUT_DIR}/${_rel_dir}")
        else()
            set(_out_dir "${YAFF_OUT_DIR}")
        endif()
        # protoc does not create output directories on its own, so we ensure
        # the directory exists at configure time.
        file(MAKE_DIRECTORY "${_out_dir}")

        set(_out_h "${_out_dir}/${_name}${_yaff_suffix}.yaff.h")
        set(_out_cpp "${_out_dir}/${_name}${_yaff_suffix}.yaff.cpp")

        if(_plugin_options_str)
            set(_yaff_out_flag "--yaff_out=${_plugin_options_str}:${YAFF_OUT_DIR}")
        else()
            set(_yaff_out_flag "--yaff_out=${YAFF_OUT_DIR}")
        endif()

        add_custom_command(
            OUTPUT "${_out_h}" "${_out_cpp}"
            COMMAND protobuf::protoc
                ${_import_flags}
                "--plugin=protoc-gen-yaff=${_plugin_bin}"
                "${_yaff_out_flag}"
                "${_abs_proto}"
            DEPENDS "${_abs_proto}" ${_plugin_dep}
            COMMENT "YaFF: Generating ${_name}${_yaff_suffix}.yaff.h and ${_name}${_yaff_suffix}.yaff.cpp"
            VERBATIM
        )
        list(APPEND _generated_files "${_out_h}" "${_out_cpp}")
    endforeach()

    set_source_files_properties(${_generated_files} PROPERTIES GENERATED TRUE)
    if(YAFF_OUT_VAR)
        set(${YAFF_OUT_VAR} ${_generated_files} PARENT_SCOPE)
    endif()

    if(YAFF_TARGET)
        target_sources(${YAFF_TARGET} PRIVATE ${_generated_files})
    endif()
endfunction()
