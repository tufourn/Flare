#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include <cstdint>
#include <vector>
#include <span>
#include <array>

#include "GpuResources.h"

struct GLFWwindow;

namespace Flare {
    static constexpr uint32_t FRAMES_IN_FLIGHT = 2;

    struct ResourcePoolCI {
        uint32_t pipelines = 256;
        uint32_t descriptorSetLayouts = 256;
        uint32_t buffers = 256;
    };

    struct GpuDeviceCreateInfo {
        GLFWwindow *glfwWindow = nullptr;
        ResourcePoolCI resourcePoolCI;
    };

    struct GpuDevice {
        void init(GpuDeviceCreateInfo &gpuDeviceCI);

        void shutdown();

        void setSurfaceFormat(VkFormat format);

        void setPresentMode(VkPresentModeKHR mode);

        void setSwapchainExtent();

        void createSwapchain();

        void destroySwapchain();

        void newFrame();

        void advanceFrameCounter();

        void present();

        VkCommandBuffer getCommandBuffer(bool begin = true);

        void reflect(ReflectOutput &reflection, const std::vector<uint32_t> &spirv,
                     const std::span<ShaderExecModel> &execModels) const;

        Handle<Pipeline> createPipeline(const PipelineCI &ci);

        void destroyPipeline(Handle<Pipeline> handle);

        Handle<DescriptorSetLayout> createDescriptorSetLayout(const VkDescriptorSetLayoutCreateInfo &ci);

        void destroyDescriptorSetLayout(Handle<DescriptorSetLayout> handle);

        Handle<Buffer> createBuffer(const BufferCI &ci);

        void destroyBuffer(Handle<Buffer> handle);

        VkInstance instance;
        VkDebugUtilsMessengerEXT debugMessenger;

        GLFWwindow *glfwWindow;
        VkSurfaceKHR surface;
        VkSwapchainKHR swapchain;
        VkSurfaceFormatKHR surfaceFormat;
        VkPresentModeKHR presentMode;
        VkExtent2D swapchainExtent;
        VkSurfaceCapabilitiesKHR surfaceCapabilities;
        std::vector<VkSurfaceFormatKHR> surfaceFormats;
        std::vector<VkPresentModeKHR> presentModes;
        std::vector<VkImage> swapchainImages;
        std::vector<VkImageView> swapchainImageViews;
        uint32_t swapchainImageIndex = 0;
        bool resized = false;

        uint32_t currentFrame = 0;
        uint64_t absoluteFrame = 0;

        std::array<VkSemaphore, FRAMES_IN_FLIGHT> imageAcquiredSemaphores;
        std::array<VkSemaphore, FRAMES_IN_FLIGHT> renderCompletedSemaphores;
        VkSemaphore graphicsTimelineSemaphore;

        std::array<VkCommandPool, FRAMES_IN_FLIGHT> commandPools;
        std::array<VkCommandBuffer, FRAMES_IN_FLIGHT> commandBuffers;

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
        ResourcePool<DescriptorSetLayout> descriptorSetLayouts;
        ResourcePool<Buffer> buffers;
    };
}
