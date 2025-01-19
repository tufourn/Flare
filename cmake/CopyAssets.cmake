function(copy_assets TargetName DirToCopy)
    get_target_property(TargetSourceDir ${TargetName} SOURCE_DIR)
    add_custom_command(
            TARGET ${TargetName} POST_BUILD
            COMMAND
            ${CMAKE_COMMAND} -E copy_directory_if_different
            ${TargetSourceDir}/${DirToCopy}
            $<TARGET_FILE_DIR:${TargetName}>/${DirToCopy}
    )
endfunction(copy_assets)