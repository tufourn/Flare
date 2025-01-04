#pragma once

#include <volk.h>

#include <cstdint>

struct GLFWwindow;

namespace Flare {
    struct GpuDeviceCreateInfo {
        uint32_t width;
        uint32_t height;
        GLFWwindow* glfwWindow;
    };

    struct GpuDevice {
        void init(GpuDeviceCreateInfo& gpuDeviceCI);
        void shutdown();

        VkInstance instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    };
}
