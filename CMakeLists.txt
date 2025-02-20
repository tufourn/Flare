cmake_minimum_required(VERSION 3.28)
project(Flare)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)

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
add_library(flare-imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_link_libraries(flare-imgui volk)
target_include_directories(flare-imgui PUBLIC ${imgui_SOURCE_DIR} ${glfw_SOURCE_DIR}/include)
target_compile_definitions(flare-imgui PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES IMGUI_IMPL_VULKAN_USE_VOLK)

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

FetchContent_Declare(
        cgltf
        GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
        GIT_TAG master
        GIT_PROGRESS true
)
FetchContent_MakeAvailable(cgltf)

FetchContent_Declare(
        spirv-cross
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
        GIT_TAG main
        GIT_PROGRESS true
)
FetchContent_MakeAvailable(spirv-cross)

add_library(flare-spirv-cross STATIC
        ${spirv-cross_SOURCE_DIR}/spirv_reflect.cpp
        ${spirv-cross_SOURCE_DIR}/spirv_cross.cpp
        ${spirv-cross_SOURCE_DIR}/spirv_parser.cpp
        ${spirv-cross_SOURCE_DIR}/spirv_cross_parsed_ir.cpp
        ${spirv-cross_SOURCE_DIR}/spirv_cfg.cpp
)

include(cmake/FetchSlang.cmake)

add_library(FlareExternal INTERFACE)
target_link_libraries(FlareExternal INTERFACE
        volk
        glfw
        flare-imgui
        TracyClient
        glm
        spdlog
        flare-spirv-cross
        slang
        Vulkan::shaderc_combined
)
target_include_directories(FlareExternal INTERFACE
        ${vma_SOURCE_DIR}/include
        ${stb_SOURCE_DIR}
        ${cgltf_SOURCE_DIR}
        ${spirv-cross_SOURCE_DIR}
)
target_compile_definitions(FlareExternal INTERFACE GLM_FORCE_DEPTH_ZERO_TO_ONE)

add_library(FlareApp
        src/Flare/FlareApp/Window.cpp
        src/Flare/FlareApp/Window.h
        src/Flare/FlareApp/Application.cpp
        src/Flare/FlareApp/Application.h
        src/Flare/FlareApp/Camera.cpp
        src/Flare/FlareApp/Camera.h
)
target_link_libraries(FlareApp PRIVATE
        FlareExternal
)

add_library(FlareGraphics
        src/Flare/FlareGraphics/GpuDevice.cpp
        src/Flare/FlareGraphics/GpuDevice.h
        src/Flare/FlareGraphics/VmaUsage.cpp
        src/Flare/FlareGraphics/GpuResources.cpp
        src/Flare/FlareGraphics/GpuResources.h
        src/Flare/FlareGraphics/ShaderCompiler.cpp
        src/Flare/FlareGraphics/ShaderCompiler.h
        src/Flare/FlareGraphics/VkHelper.cpp
        src/Flare/FlareGraphics/VkHelper.h
        src/Flare/FlareGraphics/AsyncLoader.cpp
        src/Flare/FlareGraphics/AsyncLoader.h
        src/Flare/FlareGraphics/GltfScene.cpp
        src/Flare/FlareGraphics/GltfScene.h
        src/Flare/FlareGraphics/RingBuffer.cpp
        src/Flare/FlareGraphics/RingBuffer.h
)
target_link_libraries(FlareGraphics PRIVATE
        FlareExternal
)
option(ENABLE_VULKAN_VALIDATION "Enable Vulkan Validation Layers" OFF)
target_compile_definitions(FlareGraphics PRIVATE
        $<$<BOOL:${ENABLE_VULKAN_VALIDATION}>:ENABLE_VULKAN_VALIDATION>
)

include(cmake/CopyAssets.cmake)

add_subdirectory(src/01-triangle)
add_subdirectory(src/02-textured-triangle)
add_subdirectory(src/03-gltf)
