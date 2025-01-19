#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include <cstdint>
#include <vector>

#include "GpuResources.h"

struct GLFWwindow;

namespace Flare {
    struct ResourcePoolCI {
        uint32_t pipelines = 256;
    };

    struct GpuDeviceCreateInfo {
        GLFWwindow* glfwWindow = nullptr;
        ResourcePoolCI resourcePoolCI;
    };

    struct GpuDevice {
        void init(GpuDeviceCreateInfo& gpuDeviceCI);
        void shutdown();

        void setSurfaceFormat(VkFormat format);
        void setPresentMode(VkPresentModeKHR mode);
        void setSwapchainExtent();
        void createSwapchain();
        void destroySwapchain();

        Handle<Pipeline> createPipeline(const PipelineCI& pipelineCI);

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
        std::vector<VkImageView> swapchainImageViews;

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

        VmaAllocator allocator;

        ResourcePool<Pipeline> pipelines;
    };
}
