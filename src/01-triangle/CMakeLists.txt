project(flare-triangle)

add_executable(flare-triangle
        main.cpp
)

target_include_directories(flare-triangle PRIVATE
        ../Flare
)

target_link_libraries(flare-triangle PRIVATE
        FlareExternal
        FlareApp
        FlareGraphics
)

target_copy_slang_binaries(flare-triangle)
copy_assets(flare-triangle shaders)