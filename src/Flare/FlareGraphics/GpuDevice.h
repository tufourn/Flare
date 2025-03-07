#pragma once

#define VK_NO_PROTOTYPES

#include <vk_mem_alloc.h>

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "GpuResources.h"
#include "ShaderCompiler.h"

struct GLFWwindow;

namespace Flare {
static constexpr uint32_t FRAMES_IN_FLIGHT = 2;

static constexpr uint32_t UNIFORM_BUFFERS_SET = 0;
static constexpr uint32_t STORAGE_BUFFERS_SET = 1;
static constexpr uint32_t SAMPLED_IMAGES_SET = 2;
static constexpr uint32_t STORAGE_IMAGES_SET = 3;
static constexpr uint32_t SAMPLERS_SET = 4;
static constexpr uint32_t ACCEL_STRUCT_SET = 5;
static constexpr uint32_t SET_COUNT = 6;

static constexpr size_t STAGING_BUFFER_SIZE_MB = 128;

struct ResourcePoolCI {
  uint32_t pipelines = 256;
  uint32_t storageBuffers = 512;
  uint32_t uniformBuffers = 512;
  uint32_t textures = 512;
  uint32_t storageTextures = 64;
  uint32_t samplers = 256;
};

struct BindlessSetup {
  uint32_t uniformBuffers = 512 * 1024;
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

  Handle<Pipeline> createPipeline(const PipelineCI &ci);

  void recreatePipeline(Handle<Pipeline> handle, const PipelineCI &ci);

  void destroyPipeline(Handle<Pipeline> handle);

  Handle<Buffer> createBuffer(const BufferCI &ci);

  void uploadBufferData(Handle<Buffer> targetHandle, void *data);

  void destroyBuffer(Handle<Buffer> handle);

  Buffer *getBuffer(Handle<Buffer> handle);

  Texture *getTexture(Handle<Texture> handle);

  Pipeline *getPipeline(Handle<Pipeline> handle);

  Handle<Texture> createTexture(const TextureCI &ci);

  void uploadTextureData(Texture *texture, void *data, bool genMips);

  void destroyTexture(Handle<Texture> handle);

  Handle<Sampler> createSampler(const SamplerCI &ci);

  void destroySampler(Handle<Sampler> handle);

  void createBindlessDescriptorSets(const GpuDeviceCreateInfo &ci);

  void destroyBindlessDescriptorSets();

  void createDefaultTextures();

  void destroyDefaultTextures();

  void createDrawTexture();

  void destroyDrawTexture();

  void transitionDrawTextureToColorAttachment(VkCommandBuffer cmd);

  void transitionDrawTextureToTransferSrc(VkCommandBuffer cmd);

  void transitionSwapchainTextureToTransferDst(VkCommandBuffer cmd);

  void transitionSwapchainTextureToPresentSrc(VkCommandBuffer cmd);

  void copyDrawTextureToSwapchain(VkCommandBuffer cmd);

  VkInstance instance;
  VkDebugUtilsMessengerEXT debugMessenger;

  ShaderCompiler shaderCompiler;

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
  std::array<VkDescriptorSetLayout, SET_COUNT> bindlessDescriptorSetLayouts;
  std::array<VkDescriptorSet, SET_COUNT> bindlessDescriptorSets;

  VmaAllocator allocator;

  VkFormat drawTextureFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
  Handle<Texture> drawTexture;

  Handle<Buffer> stagingBufferHandle;
  Handle<Sampler> defaultSampler;
  Handle<Texture> defaultTexture;
  Handle<Texture> defaultNormalTexture;

  ResourcePool<Pipeline> pipelines;
  ResourcePool<Buffer> storageBuffers;
  ResourcePool<Buffer> uniformBuffers;
  ResourcePool<Texture> textures;
  ResourcePool<uint8_t>
      storageTextures; // to ensure proper storage image indexing
  ResourcePool<Sampler> samplers;

  template <typename T>
  void setVkObjectName(T handle, VkObjectType type,
                       const std::string &name) const {
#ifdef ENABLE_VULKAN_VALIDATION
    const VkDebugUtilsObjectNameInfoEXT objectNameInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = type,
        .objectHandle = reinterpret_cast<uint64_t>(handle),
        .pObjectName = name.c_str(),
    };
    vkSetDebugUtilsObjectNameEXT(device, &objectNameInfo);
#endif
  }
};
} // namespace Flare
