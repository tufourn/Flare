function(sync_shaders TargetName)
    get_target_property(TargetSourceDir ${TargetName} SOURCE_DIR)
    get_target_property(TargetBinaryDir ${TargetName} BINARY_DIR)

    file(MAKE_DIRECTORY ${TargetBinaryDir}/shaders)

    file(GLOB src_shaders CONFIGURE_DEPENDS ${TargetSourceDir}/shaders/*)
    file(GLOB build_shaders CONFIGURE_DEPENDS ${TargetBinaryDir}/shaders/*)

    set(shader_copy_commands)

    # todo: copy based on timestamps

    # copy shader to build if it doesn't exist
    foreach (src_shader ${src_shaders})
        get_filename_component(shader_name ${src_shader} NAME)
        set(build_shader ${TargetBinaryDir}/shaders/${shader_name})

        if (NOT EXISTS ${build_shader})
            list(APPEND shader_copy_commands
                    COMMAND ${CMAKE_COMMAND} -E copy ${src_shader} ${build_shader}
                    COMMENT "Copying ${shader_name} from src to build"
            )
        endif ()
    endforeach ()

    # copy shader from build back to src
    foreach (build_shader ${build_shaders})
        get_filename_component(shader_name ${build_shader} NAME)
        set(src_shader ${TargetSourceDir}/shaders/${shader_name})

        if (EXISTS ${src_shader})
            list(APPEND shader_copy_commands
                    COMMAND ${CMAKE_COMMAND} -E copy ${build_shader} ${src_shader}
                    COMMENT "Copying ${shader_name} from build to src"
            )
        endif ()
    endforeach ()

    add_custom_target(${TargetName}_sync_shaders ALL
            ${shader_copy_commands}
            COMMENT "syncing shaders for ${TargetName}"
    )
    add_dependencies(${TargetName} ${TargetName}_sync_shaders)
endfunction(sync_shaders)