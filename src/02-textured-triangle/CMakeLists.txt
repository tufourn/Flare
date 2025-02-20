project(flare-textured-triangle)

add_executable(flare-textured-triangle
        main.cpp
)

target_include_directories(flare-textured-triangle PRIVATE
        ../Flare
)

target_link_libraries(flare-textured-triangle PRIVATE
        FlareExternal
        FlareApp
        FlareGraphics
)

target_copy_slang_binaries(flare-textured-triangle)
copy_assets(flare-textured-triangle shaders assets)
