#pragma once

#include <volk.h>

#include <cstdint>
#include <vector>

struct GLFWwindow;

namespace Flare {
    struct GpuDeviceCreateInfo {
        GLFWwindow* glfwWindow;
    };

    struct GpuDevice {
        void init(GpuDeviceCreateInfo& gpuDeviceCI);
        void shutdown();

        void setSurfaceFormat(VkFormat format);
        void setPresentMode(VkPresentModeKHR mode);
        void setSwapchainExtent();
        void createSwapchain();

        VkInstance instance;
        VkDebugUtilsMessengerEXT debugMessenger;

        GLFWwindow* glfwWindow;
        VkSurfaceKHR surface;
        VkSwapchainKHR swapchain;
        VkSurfaceFormatKHR surfaceFormat;
        VkPresentModeKHR presentMode;
        VkExtent2D swapchainExtent;
        VkSurfaceCapabilitiesKHR surfaceCapabilities;
        std::vector<VkSurfaceFormatKHR> surfaceFormats;
        std::vector<VkPresentModeKHR> presentModes;

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
