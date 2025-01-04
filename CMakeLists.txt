cmake_minimum_required(VERSION 3.28)
project(Flare)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(
        volk
        GIT_REPOSITORY https://github.com/zeux/volk.git
        GIT_TAG master
        GIT_PROGRESS true
)
FetchContent_MakeAvailable(volk)

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG master
        GIT_PROGRESS true
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG master
        GIT_PROGRESS true
)
FetchContent_MakeAvailable(imgui)
add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_link_libraries(imgui volk)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${glfw_SOURCE_DIR}/include)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES IMGUI_IMPL_VULKAN_USE_VOLK)

FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(tracy)

FetchContent_Declare(
        vma
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG master
        GIT_PROGRESS true
)
FetchContent_MakeAvailable(vma)

FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG master
        GIT_PROGRESS true
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master
        GIT_PROGRESS true
)
FetchContent_MakeAvailable(stb)

FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.x
        GIT_PROGRESS true
)
FetchContent_MakeAvailable(spdlog)

add_library(FlareExternal INTERFACE)
target_link_libraries(FlareExternal INTERFACE
        volk
        glfw
        imgui
        TracyClient
        glm
        spdlog
)
target_include_directories(FlareExternal INTERFACE
        ${vma_SOURCE_DIR}/include
        ${stb_SOURCE_DIR}
)

add_library(FlareApp
        src/Flare/FlareApp/Window.cpp
        src/Flare/FlareApp/Window.h
        src/Flare/FlareApp/Application.cpp
        src/Flare/FlareApp/Application.h
)
target_link_libraries(FlareApp PRIVATE
        FlareExternal
)

add_subdirectory(src/01-triangle)
