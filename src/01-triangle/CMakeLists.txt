project(triangle)

add_executable(triangle
        main.cpp
)

target_include_directories(triangle PRIVATE
        ../Flare
)

target_link_libraries(triangle PRIVATE
        FlareExternal
        FlareApp
)