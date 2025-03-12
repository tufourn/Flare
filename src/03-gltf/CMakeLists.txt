project(flare-gltf)

add_executable(flare-gltf
        main.cpp
)

target_include_directories(flare-gltf PRIVATE
        ../Flare
)

target_link_libraries(flare-gltf PRIVATE
        FlareExternal
        FlareApp
        FlareGraphics
)

target_copy_slang_binaries(flare-gltf)
copy_assets(flare-gltf ../Flare/FlareGraphics/CoreShaders)
#sync_shaders(flare-gltf)
