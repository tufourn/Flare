function(copy_assets TargetName)
    get_target_property(TargetSourceDir ${TargetName} SOURCE_DIR)

    set(CopyCommands)
    foreach (DirToCopy IN LISTS ARGN)
        # copy from source to build
        list(APPEND CopyCommands
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${TargetSourceDir}/${DirToCopy}
                $<TARGET_FILE_DIR:${TargetName}>/${DirToCopy}
        )

    endforeach ()

    add_custom_target(${TargetName}_copy_assets ALL
            ${CopyCommands}
            COMMENT "copying assets for ${TargetName}"
    )

    add_dependencies(${TargetName} ${TargetName}_copy_assets)
endfunction(copy_assets)