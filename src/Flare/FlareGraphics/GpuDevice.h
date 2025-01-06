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

        VkInstance instance;
        VkDebugUtilsMessengerEXT debugMessenger;
        VkPhysicalDevice physicalDevice;
        VkPhysicalDeviceProperties physicalDeviceProperties;
        VkPhysicalDeviceFeatures physicalDeviceFeatures;
        VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
        VkDevice device;
        VkQueue mainQueue;
        VkQueue computeQueue;
        VkQueue transferQueue;
        uint32_t mainFamily;
        uint32_t computeFamily;
        uint32_t transferFamily;
    };
}
