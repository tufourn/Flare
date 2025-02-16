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

    static constexpr uint32_t STORAGE_BUFFERS_SET = 0;
    static constexpr uint32_t SAMPLED_IMAGES_SET = 1;
    static constexpr uint32_t STORAGE_IMAGES_SET = 2;
    static constexpr uint32_t SAMPLERS_SET = 3;
    static constexpr uint32_t ACCEL_STRUCT_SET = 4;

    struct PushConstants {
        uint32_t bindingsOffset;
        uint32_t data0;
    };

    struct ResourcePoolCI {
        uint32_t pipelines = 256;
        uint32_t buffers = 256;
        uint32_t textures = 256;
        uint32_t samplers = 256;
    };

    struct BindlessSetup {
        uint32_t storageBuffers = 512 * 1024;
        uint32_t sampledImages = 512 * 1024;
        uint32_t storageImages = 64 * 1024;
        uint32_t accelStructs = 32 * 1024;
        uint32_t samplers = 4 * 1024;
    };

    struct GpuDeviceCreateInfo {
        GLFWwindow *glfwWindow = nullptr;
        ResourcePoolCI resourcePoolCI;
        BindlessSetup bindlessSetup;
    };

    struct GpuDevice {
        void init(GpuDeviceCreateInfo &gpuDeviceCI);

        void shutdown();

        void setSurfaceFormat(VkFormat format);

        void setPresentMode(VkPresentModeKHR mode);

        void setSwapchainExtent();

        void createSwapchain();

        void destroySwapchain();

        void resizeSwapchain();

        void newFrame();

        void advanceFrameCounter();

        void present();

        VkCommandBuffer getCommandBuffer(bool begin = true);

        void submitImmediate(VkCommandBuffer cmd);

        void reflect(ReflectOutput &reflection, const std::vector<uint32_t> &spirv,
                     const std::span<ShaderExecModel> &execModels) const; // todo: doesn't need to be in this class

        Handle<Pipeline> createPipeline(const PipelineCI &ci);

        void destroyPipeline(Handle<Pipeline> handle);

        Handle<Buffer> createBuffer(const BufferCI &ci);

        void destroyBuffer(Handle<Buffer> handle);

        Handle<Texture> createTexture(const TextureCI &ci);

        void destroyTexture(Handle<Texture> handle);

        Handle<Sampler> createSampler(const SamplerCI &ci);

        void destroySampler(Handle<Sampler> handle);

        void createBindlessDescriptorSets(const GpuDeviceCreateInfo& ci);

        void destroyBindlessDescriptorSets();

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
        std::vector<Handle<Texture>> depthTextures;
        uint32_t swapchainImageIndex = 0;
        bool resized = false;

        uint32_t currentFrame = 0;
        uint64_t absoluteFrame = 0;

        std::array<VkSemaphore, FRAMES_IN_FLIGHT> imageAcquiredSemaphores;
        std::array<VkSemaphore, FRAMES_IN_FLIGHT> renderCompletedSemaphores;
        VkSemaphore graphicsTimelineSemaphore;

        std::array<VkCommandPool, FRAMES_IN_FLIGHT> commandPools;
        std::array<VkCommandBuffer, FRAMES_IN_FLIGHT> commandBuffers;
        VkFence immediateFence;

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

        VkDescriptorPool bindlessDescriptorPool;
        std::array<VkDescriptorSetLayout, 5> bindlessDescriptorSetLayouts;
        std::array<VkDescriptorSet, 5> bindlessDescriptorSets;

        VmaAllocator allocator;

        Handle<Sampler> defaultSampler;
        Handle<Texture> defaultTexture;

        ResourcePool<Pipeline> pipelines;
        ResourcePool<Buffer> buffers;
        ResourcePool<Texture> textures;
        ResourcePool<Sampler> samplers;
    };
}
