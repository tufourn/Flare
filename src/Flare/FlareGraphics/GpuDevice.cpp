#include "GpuDevice.h"

#define GLFW_INCLUDE_NONE
#define CGLTF_IMPLEMENTATION

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cgltf.h>
#include <optional>
#include <spdlog/spdlog.h>
#include <spirv_cross.hpp>
#include <volk.h>

#include "VkHelper.h"

namespace Flare {
static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
              VkDebugUtilsMessageTypeFlagsEXT type,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void *pUserData) {
  spdlog::level::level_enum log_level;

  switch (severity) {
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
    log_level = spdlog::level::trace;
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
    log_level = spdlog::level::info;
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
    log_level = spdlog::level::warn;
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
    log_level = spdlog::level::err;
    break;
  default:
    log_level = spdlog::level::info;
    break;
  }

  spdlog::log(log_level, "Vulkan Validation: [{}] {}",
              pCallbackData->pMessageIdName, pCallbackData->pMessage);

  return VK_FALSE;
}

void GpuDevice::init(GpuDeviceCreateInfo &gpuDeviceCI) {
  spdlog::info("GpuDevice: Initialize");

  // Set window
  glfwWindow = gpuDeviceCI.glfwWindow;

  // Shader compiler
  shaderCompiler.init();

  // Init resource pools;
  pipelines.init(gpuDeviceCI.resourcePoolCI.pipelines);
  storageBuffers.init(gpuDeviceCI.resourcePoolCI.storageBuffers);
  uniformBuffers.init(gpuDeviceCI.resourcePoolCI.uniformBuffers);
  textures.init(gpuDeviceCI.resourcePoolCI.textures);
  storageTextures.init(gpuDeviceCI.resourcePoolCI.storageTextures);
  samplers.init(gpuDeviceCI.resourcePoolCI.samplers);

  // Instance
  if (volkInitialize() != VK_SUCCESS) {
    spdlog::error("GpuDevice: Failed to initialize volk");
  }

  VkApplicationInfo appInfo{
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pNext = nullptr,
      .pApplicationName = "Flare",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_3,
  };

  uint32_t glfwExtensionsCount = 0;
  const char **glfwExtensions;
  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);

  std::vector<const char *> enabledInstanceExtensions(
      glfwExtensions, glfwExtensions + glfwExtensionsCount);
  std::vector<const char *> enabledLayers;

#ifdef ENABLE_VULKAN_VALIDATION
  spdlog::info("GpuDevice: Using vulkan validation layer");

  enabledInstanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  enabledLayers.emplace_back("VK_LAYER_KHRONOS_validation");

  uint32_t layerCount;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  for (const char *layerName : enabledLayers) {
    bool found = false;
    for (const auto &layerProperties : availableLayers) {
      if (strcmp(layerName, layerProperties.layerName) == 0) {
        found = true;
      }
    }
    if (!found) {
      spdlog::error("GpuDevice: layer {} requested but not available",
                    layerName);
    }
  }

  VkDebugUtilsMessengerCreateInfoEXT debugMessengerCI{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .pNext = nullptr,
      .flags = 0,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = debugCallback,
      .pUserData = nullptr,
  };
#endif

  VkInstanceCreateInfo instanceCI{
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
#ifdef ENABLE_VULKAN_VALIDATION
      .pNext = &debugMessengerCI,
#endif
      .flags = 0,
      .pApplicationInfo = &appInfo,
      .enabledLayerCount = static_cast<uint32_t>(enabledLayers.size()),
      .ppEnabledLayerNames =
          enabledLayers.empty() ? nullptr : enabledLayers.data(),
      .enabledExtensionCount =
          static_cast<uint32_t>(enabledInstanceExtensions.size()),
      .ppEnabledExtensionNames = enabledInstanceExtensions.data(),
  };

  if (vkCreateInstance(&instanceCI, nullptr, &instance) != VK_SUCCESS) {
    spdlog::error("GpuDevice: Failed to create Vulkan instance");
  }

  volkLoadInstance(instance);

#ifdef ENABLE_VULKAN_VALIDATION
  if (vkCreateDebugUtilsMessengerEXT(instance, &debugMessengerCI, nullptr,
                                     &debugMessenger) != VK_SUCCESS) {
    spdlog::error("GpuDevice: Failed to set up debug messenger");
  }
#endif

  // Physical device
  uint32_t gpuCount = 0;
  if (vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr) != VK_SUCCESS) {
    spdlog::error("GpuDevice: Failed to enumerate physical devices");
  }
  if (gpuCount == 0) {
    spdlog::error("GpuDevice: No physical device with Vulkan support");
  }
  std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
  if (vkEnumeratePhysicalDevices(instance, &gpuCount, physicalDevices.data()) !=
      VK_SUCCESS) {
    spdlog::error("GpuDevice: Failed to enumerate physical devices");
  }

  VkPhysicalDevice discreteGpu = VK_NULL_HANDLE;
  VkPhysicalDevice integratedGpu = VK_NULL_HANDLE;
  for (size_t i = 0; i < gpuCount; i++) {
    VkPhysicalDeviceProperties gpuProperties;
    vkGetPhysicalDeviceProperties(physicalDevices[i], &gpuProperties);
    if (gpuProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      discreteGpu = physicalDevices[i];
      continue;
    }
    if (gpuProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
      integratedGpu = physicalDevices[i];
      continue;
    }
  }

  if (discreteGpu != VK_NULL_HANDLE) {
    physicalDevice = discreteGpu;
  } else if (integratedGpu != VK_NULL_HANDLE) {
    physicalDevice = integratedGpu;
  } else {
    spdlog::error("GpuDevice: Failed to find a suitable gpu");
  }

  vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
  vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);
  vkGetPhysicalDeviceMemoryProperties(physicalDevice,
                                      &physicalDeviceMemoryProperties);
  spdlog::info("GpuDevice: Using {}", physicalDeviceProperties.deviceName);

  // queues, separate family for each queue, todo: same family for queues in
  // case can't find separate family
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                           nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                           queueFamilies.data());

  std::optional<uint32_t> mainFamilyOpt;
  std::optional<uint32_t> computeFamilyOpt;
  std::optional<uint32_t> transferFamilyOpt;

  for (uint32_t fi = 0; fi < queueFamilyCount; fi++) {
    VkQueueFamilyProperties family = queueFamilies[fi];

    if (family.queueCount == 0) {
      continue;
    }

    if (!mainFamilyOpt.has_value() &&
        (family.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) ==
            (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
      mainFamilyOpt = fi;
      continue;
    }

    if (!computeFamilyOpt.has_value() &&
        family.queueFlags & VK_QUEUE_COMPUTE_BIT) {
      computeFamilyOpt = fi;
      continue;
    }

    if (!transferFamilyOpt.has_value() &&
        ((family.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0) &&
        family.queueFlags & VK_QUEUE_TRANSFER_BIT) {
      transferFamilyOpt = fi;
      continue;
    }

    if (mainFamilyOpt.has_value() && computeFamilyOpt.has_value() &&
        transferFamilyOpt.has_value()) {
      break;
    }
  }

  if (!mainFamilyOpt.has_value()) {
    spdlog::error("GpuDevice: Failed to find main queue family");
  }
  if (!computeFamilyOpt.has_value()) {
    spdlog::error("GpuDevice: Failed to find compute queue family");
  }
  if (!transferFamilyOpt.has_value()) {
    spdlog::error("GpuDevice: Failed to find transfer queue family");
  }

  mainFamily = mainFamilyOpt.value();
  computeFamily = computeFamilyOpt.value();
  transferFamily = transferFamilyOpt.value();

  float queuePriority = 1.f;

  VkDeviceQueueCreateInfo mainQueueCI{
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .queueFamilyIndex = mainFamily,
      .queueCount = 1,
      .pQueuePriorities = &queuePriority,
  };

  VkDeviceQueueCreateInfo computeQueueCI{
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .queueFamilyIndex = computeFamily,
      .queueCount = 1,
      .pQueuePriorities = &queuePriority,
  };

  VkDeviceQueueCreateInfo transferQueueCI{
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .queueFamilyIndex = transferFamily,
      .queueCount = 1,
      .pQueuePriorities = &queuePriority,
  };

  std::vector<VkDeviceQueueCreateInfo> queueCI = {mainQueueCI, computeQueueCI,
                                                  transferQueueCI};

  // device extensions
  uint32_t deviceExtensionCount = 0;
  vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr,
                                       &deviceExtensionCount, nullptr);
  std::vector<VkExtensionProperties> deviceExtensions(deviceExtensionCount);
  vkEnumerateDeviceExtensionProperties(
      physicalDevice, nullptr, &deviceExtensionCount, deviceExtensions.data());

  std::vector<const char *> enabledDeviceExtensions;

  swapchainExtensionPresent = false;
  accelStructExtensionPresent = false;
  deferredHostOpExtensionPresent = false;

  for (size_t i = 0; i < deviceExtensionCount; i++) {
    if (strcmp(deviceExtensions[i].extensionName,
               VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
      swapchainExtensionPresent = true;
    }
    if (strcmp(deviceExtensions[i].extensionName,
               VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) == 0) {
      accelStructExtensionPresent = true;
    }
    if (strcmp(deviceExtensions[i].extensionName,
               VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME) == 0) {
      deferredHostOpExtensionPresent = true;
    }
  }

  if (!swapchainExtensionPresent) {
    spdlog::error("GpuDevice: Swapchain extension not present");
  }
  if (accelStructExtensionPresent) {
    enabledDeviceExtensions.emplace_back(
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
  }
  if (deferredHostOpExtensionPresent) {
    enabledDeviceExtensions.emplace_back(
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
  }

  enabledDeviceExtensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  // device features
  VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
      .pNext = nullptr};

  VkPhysicalDeviceSynchronization2Features synchronization2Features{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
      .pNext = &dynamicRenderingFeatures,
  };

  VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParamFeatures{
      .sType =
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,
      .pNext = &synchronization2Features,
  };

  VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStructFeatures{
      .sType =
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
      .pNext = &shaderDrawParamFeatures,
  };

  VkPhysicalDeviceVulkan12Features features12 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext = accelStructExtensionPresent
                   ? static_cast<void *>(&accelStructFeatures)
                   : static_cast<void *>(&shaderDrawParamFeatures),
      .drawIndirectCount = VK_TRUE,
      .descriptorIndexing = VK_TRUE,
      .timelineSemaphore = VK_TRUE,
      .bufferDeviceAddress = VK_TRUE,
  };

  VkPhysicalDeviceFeatures2 features = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .pNext = &features12,
  };

  vkGetPhysicalDeviceFeatures2(physicalDevice, &features);

  // logical device creation
  VkDeviceCreateInfo deviceCI{
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = &features,
      .flags = 0,
      .queueCreateInfoCount = static_cast<uint32_t>(queueCI.size()),
      .pQueueCreateInfos = queueCI.data(),
      .enabledExtensionCount =
          static_cast<uint32_t>(enabledDeviceExtensions.size()),
      .ppEnabledExtensionNames = enabledDeviceExtensions.data(),
  };

  if (vkCreateDevice(physicalDevice, &deviceCI, nullptr, &device) !=
      VK_SUCCESS) {
    spdlog::error("GpuDevice: Failed to create logical device");
  }

  volkLoadDevice(device);

  vkGetDeviceQueue(device, mainFamily, 0, &mainQueue);
  vkGetDeviceQueue(device, computeFamily, 0, &computeQueue);
  vkGetDeviceQueue(device, transferFamily, 0, &transferQueue);

  // vma
  VmaVulkanFunctions vmaFunctions = {
      .vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
      .vkGetPhysicalDeviceMemoryProperties =
          vkGetPhysicalDeviceMemoryProperties,
      .vkAllocateMemory = vkAllocateMemory,
      .vkFreeMemory = vkFreeMemory,
      .vkMapMemory = vkMapMemory,
      .vkUnmapMemory = vkUnmapMemory,
      .vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
      .vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
      .vkBindBufferMemory = vkBindBufferMemory,
      .vkBindImageMemory = vkBindImageMemory,
      .vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
      .vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
      .vkCreateBuffer = vkCreateBuffer,
      .vkDestroyBuffer = vkDestroyBuffer,
      .vkCreateImage = vkCreateImage,
      .vkDestroyImage = vkDestroyImage,
      .vkCmdCopyBuffer = vkCmdCopyBuffer,
  };

  VmaAllocatorCreateInfo allocatorCI = {
      .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
      .physicalDevice = physicalDevice,
      .device = device,
      .pVulkanFunctions = &vmaFunctions,
      .instance = instance,
  };

  if (vmaCreateAllocator(&allocatorCI, &allocator) != VK_SUCCESS) {
    spdlog::error("GpuDevice: Failed to create VmaAllocator");
  }

  vmaCreateAllocator(&allocatorCI, &allocator);

  // bindless descriptor sets
  createBindlessDescriptorSets(gpuDeviceCI);

  // surface and swapchain
  if (glfwCreateWindowSurface(instance, glfwWindow, nullptr, &surface) !=
      VK_SUCCESS) {
    spdlog::error("GpuDevice: Failed to create window surface");
  }

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                            &surfaceCapabilities);

  uint32_t surfaceFormatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface,
                                       &surfaceFormatCount, nullptr);
  if (surfaceFormatCount != 0) {
    surfaceFormats.resize(surfaceFormatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        physicalDevice, surface, &surfaceFormatCount, surfaceFormats.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface,
                                            &presentModeCount, nullptr);
  if (presentModeCount != 0) {
    presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        physicalDevice, surface, &presentModeCount, presentModes.data());
  }

  if (surfaceFormats.empty() || presentModes.empty()) {
    spdlog::error("GpuDevice: Swapchain not supported");
  }

  setSurfaceFormat(VK_FORMAT_B8G8R8A8_SRGB);
  setPresentMode(VK_PRESENT_MODE_MAILBOX_KHR);
  setSwapchainExtent();
  createSwapchain();

  createDrawTexture();

  VkFenceCreateInfo fenceCI = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
  };

  vkCreateFence(device, &fenceCI, nullptr, &immediateFence);

  VkSemaphoreCreateInfo semaphoreCI = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };

  // we need these per-frame semaphores because vulkan wsi swapchain doesn't
  // support timeline semaphores
  for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
    vkCreateSemaphore(device, &semaphoreCI, nullptr,
                      &imageAcquiredSemaphores[i]);
    vkCreateSemaphore(device, &semaphoreCI, nullptr,
                      &renderCompletedSemaphores[i]);
  }

  // using timeline semaphores for synchronization
  VkSemaphoreTypeCreateInfo semaphoreTypeCI = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
      .pNext = nullptr,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
      .initialValue = 0,
  };
  semaphoreCI.pNext = &semaphoreTypeCI;
  vkCreateSemaphore(device, &semaphoreCI, nullptr, &graphicsTimelineSemaphore);

  VkCommandPoolCreateInfo commandPoolCI = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = mainFamily, // todo: only main queue for now
  };

  VkCommandBufferAllocateInfo commandAllocInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = VK_NULL_HANDLE,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
    if (vkCreateCommandPool(device, &commandPoolCI, nullptr,
                            &commandPools[i]) != VK_SUCCESS) {
      spdlog::error("Failed to create command pool");
    }

    commandAllocInfo.commandPool = commandPools[i];

    if (vkAllocateCommandBuffers(device, &commandAllocInfo,
                                 &commandBuffers[i]) != VK_SUCCESS) {
      spdlog::error("Failed to allocated command buffer");
    }
  }

  BufferCI stagingBufferCI = {
      .size = STAGING_BUFFER_SIZE_MB * 1024 * 1024,
      .usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .mapped = true,
  };

  stagingBufferHandle = createBuffer(stagingBufferCI);

  createDefaultTextures();
}

void GpuDevice::shutdown() {
  vkDeviceWaitIdle(device);

  shaderCompiler.shutdown();

  destroyDefaultTextures();
  destroyBuffer(stagingBufferHandle);

  destroyBindlessDescriptorSets();

  for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
    vkDestroyCommandPool(device, commandPools[i], nullptr);
  }

  vkDestroyFence(device, immediateFence, nullptr);
  vkDestroySemaphore(device, graphicsTimelineSemaphore, nullptr);
  for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
    vkDestroySemaphore(device, imageAcquiredSemaphores[i], nullptr);
    vkDestroySemaphore(device, renderCompletedSemaphores[i], nullptr);
  }

  destroyDrawTexture();
  destroySwapchain();
  vmaDestroyAllocator(allocator);
  vkDestroyDevice(device, nullptr);
  vkDestroySurfaceKHR(instance, surface, nullptr);
#ifdef ENABLE_VULKAN_VALIDATION
  vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
#endif
  vkDestroyInstance(instance, nullptr);
  spdlog::info("GpuDevice: Shutdown");
}

void GpuDevice::setSurfaceFormat(VkFormat format) {
  bool supported = false;

  for (const auto &availableFormat : surfaceFormats) {
    if (availableFormat.format == format &&
        availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      surfaceFormat = availableFormat;
      supported = true;
    }
  }

  if (!supported) {
    spdlog::error("GpuDevice: Surface format not supported");
    surfaceFormat = surfaceFormats[0];
  }
}

void GpuDevice::setPresentMode(VkPresentModeKHR mode) {
  bool supported = false;
  for (const auto &availableMode : presentModes) {
    if (availableMode == mode) {
      presentMode = availableMode;
      supported = true;
    }
  }

  if (!supported) {
    spdlog::error("GpuDevice: Present mode not supported, using FIFO");
    presentMode = VK_PRESENT_MODE_FIFO_KHR;
  }
}

void GpuDevice::setSwapchainExtent() {
  if (!glfwWindow) {
    spdlog::error("GpuDevice: Window not set, can't set swapchain extent");
    return;
  }

  if (surfaceCapabilities.currentExtent.width !=
          std::numeric_limits<uint32_t>::max() &&
      surfaceCapabilities.currentExtent.height !=
          std::numeric_limits<uint32_t>::max()) {
    swapchainExtent = surfaceCapabilities.currentExtent;
  } else {
    int width, height;
    glfwGetFramebufferSize(glfwWindow, &width, &height);

    VkExtent2D actualExtent = {static_cast<uint32_t>(width),
                               static_cast<uint32_t>(height)};

    actualExtent.width =
        std::clamp(actualExtent.width, surfaceCapabilities.minImageExtent.width,
                   surfaceCapabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height,
                                     surfaceCapabilities.minImageExtent.height,
                                     surfaceCapabilities.maxImageExtent.height);

    swapchainExtent = actualExtent;
  }
}

void GpuDevice::createSwapchain() {
  uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
  if (surfaceCapabilities.maxImageCount > 0 &&
      imageCount > surfaceCapabilities.maxImageCount) {
    imageCount = surfaceCapabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR swapchainCI{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .pNext = nullptr,
      .flags = 0,
      .surface = surface,
      .minImageCount = imageCount,
      .imageFormat = surfaceFormat.format,
      .imageColorSpace = surfaceFormat.colorSpace,
      .imageExtent = swapchainExtent,
      .imageArrayLayers = 1,
      .imageUsage =
          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
      .preTransform = surfaceCapabilities.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = presentMode,
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE,
  };

  if (vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &swapchain) !=
      VK_SUCCESS) {
    spdlog::error("GpuDevice: Failed to create swapchain");
  }

  vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
  depthTextures.resize(imageCount);
  swapchainImageViews.resize(imageCount);
  swapchainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(device, swapchain, &imageCount,
                          swapchainImages.data());

  TextureCI depthTextureCI = {
      .width = swapchainExtent.width,
      .height = swapchainExtent.height,
      .depth = 1,
      .format = VK_FORMAT_D32_SFLOAT,
      .type = VK_IMAGE_TYPE_2D,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
  };

  for (size_t i = 0; i < imageCount; i++) {
    VkImageViewCreateInfo imageViewCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = swapchainImages[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = surfaceFormat.format,
        .components = VkHelper::identityRGBA(),
        .subresourceRange = VkHelper::subresourceRange(),
    };

    if (vkCreateImageView(device, &imageViewCI, nullptr,
                          &swapchainImageViews[i]) != VK_SUCCESS) {
      spdlog::error("GpuDevice: Failed to create swapchain image view");
    }

    depthTextures[i] = createTexture(depthTextureCI);
  }
}

void GpuDevice::destroySwapchain() {
  for (size_t i = 0; i < swapchainImageViews.size(); i++) {
    vkDestroyImageView(device, swapchainImageViews[i], nullptr);
    destroyTexture(depthTextures[i]);
  }
  vkDestroySwapchainKHR(device, swapchain, nullptr);
}

Handle<Pipeline> GpuDevice::createPipeline(const PipelineCI &ci) {
  std::vector<VkShaderModule> modules;
  std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

  bool shaderCompileSuccess = true;
  bool isCompute = false;

  for (const auto &shaderStage : ci.shaderStages) {
    if (shaderStage.stage == VK_SHADER_STAGE_COMPUTE_BIT) {
      isCompute = true;
    }

    std::vector<uint32_t> spirv = shaderCompiler.compileGLSL(shaderStage.path);
    if (spirv.empty()) {
      spdlog::error("Failed to compile {}", shaderStage.path.string());
      shaderCompileSuccess = false;
      break;
    }

    VkShaderModuleCreateInfo shaderModuleCI{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = spirv.size() * sizeof(uint32_t),
        .pCode = spirv.data(),
    };

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &shaderModuleCI, nullptr, &shaderModule) !=
        VK_SUCCESS) {
      spdlog::error("Failed to create shader module");
      shaderCompileSuccess = false;
      break;
    }
    modules.push_back(shaderModule);

    shaderStages.emplace_back(VkPipelineShaderStageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = shaderStage.stage,
        .module = shaderModule,
        .pName = "main",
        .pSpecializationInfo = nullptr,
    });
  }

  if (!shaderCompileSuccess) {
    for (const auto &module : modules) {
      vkDestroyShaderModule(device, module, nullptr);
    }
    return {}; // return invalid pipeline handle
  }

  Handle<Pipeline> handle = pipelines.obtain();
  Pipeline *pipeline = pipelines.get(handle);

  VkPushConstantRange pcRange = {
      .stageFlags = VK_SHADER_STAGE_ALL,
      .offset = 0,
      .size = sizeof(PushConstants),
  };

  VkPipelineLayoutCreateInfo pipelineLayoutCI = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .setLayoutCount =
          static_cast<uint32_t>(bindlessDescriptorSetLayouts.size()),
      .pSetLayouts = bindlessDescriptorSetLayouts.data(),
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &pcRange,
  };

  if (vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr,
                             &pipeline->pipelineLayout) != VK_SUCCESS) {
    spdlog::error("Failed to create pipeline layout");
  }

  if (!isCompute) {
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamicStateCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()};

    VkPipelineVertexInputStateCreateInfo vertexInputCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount =
            static_cast<uint32_t>(ci.vertexInput.vertexBindings.size()),
        .pVertexBindingDescriptions = ci.vertexInput.vertexBindings.data(),
        .vertexAttributeDescriptionCount =
            static_cast<uint32_t>(ci.vertexInput.vertexAttributes.size()),
        .pVertexAttributeDescriptions = ci.vertexInput.vertexAttributes.data(),
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkViewport viewport = {
        .x = 0.f,
        .y = 0.f,
        .width = static_cast<float>(swapchainExtent.width),
        .height = static_cast<float>(swapchainExtent.height),
        .minDepth = 0.f,
        .maxDepth = 1.f,
    };

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = swapchainExtent,
    };

    VkPipelineViewportStateCreateInfo viewportStateCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    VkPipelineRasterizationStateCreateInfo rasterizerCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = ci.rasterization.cullMode,
        .frontFace = ci.rasterization.frontFace,
        .depthBiasEnable = ci.rasterization.depthBiasEnable,
        .depthBiasConstantFactor = ci.rasterization.depthBiasConstant,
        .depthBiasClamp = 0.f,
        .depthBiasSlopeFactor = ci.rasterization.depthBiasSlope,
        .lineWidth = 1.f,
    };

    // todo: implement multisampling
    VkPipelineMultisampleStateCreateInfo multisampleCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };

    VkPipelineDepthStencilStateCreateInfo depthStencilCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthTestEnable = ci.depthStencil.depthTestEnable ? VK_TRUE : VK_FALSE,
        .depthWriteEnable =
            ci.depthStencil.depthWriteEnable ? VK_TRUE : VK_FALSE,
        .depthCompareOp = ci.depthStencil.depthCompareOp,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable =
            ci.depthStencil.stencilTestEnable ? VK_TRUE : VK_FALSE,
        .front = ci.depthStencil.front,
        .back = ci.depthStencil.back,
        .minDepthBounds = 0.f,
        .maxDepthBounds = 1.f,
    };

    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
    if (ci.colorBlend.attachments.empty()) {
      // by default, disable blend for all color attachments
      for (size_t i = 0; i < ci.rendering.colorFormats.size(); i++) {
        colorBlendAttachments.emplace_back(VkPipelineColorBlendAttachmentState{
            .blendEnable = VK_FALSE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        });
      }
    } else {
      colorBlendAttachments.reserve(ci.colorBlend.attachments.size());
      for (const auto &attachment : ci.colorBlend.attachments) {
        colorBlendAttachments.emplace_back(VkPipelineColorBlendAttachmentState{
            .blendEnable = attachment.enable ? VK_TRUE : VK_FALSE,
            .srcColorBlendFactor = attachment.srcColor,
            .dstColorBlendFactor = attachment.dstColor,
            .colorBlendOp = attachment.colorOp,
            .srcAlphaBlendFactor = attachment.srcAlpha,
            .dstAlphaBlendFactor = attachment.dstAlpha,
            .alphaBlendOp = attachment.alphaOp,
            .colorWriteMask = attachment.colorWriteMask,
        });
      }
    }

    VkPipelineColorBlendStateCreateInfo colorBlendCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size()),
        .pAttachments = colorBlendAttachments.data(),
        .blendConstants = {0.f, 0.f, 0.f, 0.f},
    };

    VkPipelineRenderingCreateInfo renderingCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,
        .viewMask = 0,
        .colorAttachmentCount =
            static_cast<uint32_t>(ci.rendering.colorFormats.size()),
        .pColorAttachmentFormats = ci.rendering.colorFormats.empty()
                                       ? nullptr
                                       : ci.rendering.colorFormats.data(),
        .depthAttachmentFormat = ci.rendering.depthFormat,
        .stencilAttachmentFormat = ci.rendering.stencilFormat,
    };

    VkGraphicsPipelineCreateInfo pipelineCI = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingCI,
        .flags = 0,
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputCI,
        .pInputAssemblyState = &inputAssemblyCI,
        .pTessellationState = nullptr, // not using tesselation
        .pViewportState = &viewportStateCI,
        .pRasterizationState = &rasterizerCI,
        .pMultisampleState = &multisampleCI,
        .pDepthStencilState = &depthStencilCI,
        .pColorBlendState = &colorBlendCI,
        .pDynamicState = &dynamicStateCI,
        .layout = pipeline->pipelineLayout,
        .renderPass = VK_NULL_HANDLE, // dynamic rendering
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    };

    if (vkCreateGraphicsPipelines(
            device, VK_NULL_HANDLE, // todo: pipeline cache
            1, &pipelineCI, nullptr, &pipeline->pipeline) != VK_SUCCESS) {
      spdlog::error("Failed to create graphics pipeline");
    }
    pipeline->bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  } else {                            // compute
    assert(shaderStages.size() == 1); // only compute stage
    VkComputePipelineCreateInfo pipelineCI = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = shaderStages[0],
        .layout = pipeline->pipelineLayout,
    };
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, // todo: pipeline cache
                                 1, &pipelineCI, nullptr,
                                 &pipeline->pipeline) != VK_SUCCESS) {
      spdlog::error("Failed to create compute pipeline");
    }
    pipeline->bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
  }

  for (const auto &module : modules) {
    vkDestroyShaderModule(device, module, nullptr);
  }

  return handle;
}

void GpuDevice::destroyPipeline(Handle<Pipeline> handle) {
  if (!handle.isValid()) {
    spdlog::error("Invalid pipeline handle");
    return;
  }

  Pipeline *pipeline = pipelines.get(handle);

  vkDestroyPipelineLayout(device, pipeline->pipelineLayout, nullptr);

  vkDestroyPipeline(device, pipeline->pipeline, nullptr);

  pipelines.release(handle);
}

void GpuDevice::newFrame() {
  if (absoluteFrame >= FRAMES_IN_FLIGHT) {
    uint64_t graphicsTimelineWaitValue = absoluteFrame - FRAMES_IN_FLIGHT + 1;

    VkSemaphoreWaitInfo waitInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .pNext = nullptr,
        .flags = 0,
        .semaphoreCount = 1,
        .pSemaphores = &graphicsTimelineSemaphore,
        .pValues = &graphicsTimelineWaitValue,
    };

    vkWaitSemaphores(device, &waitInfo, UINT64_MAX);
  }

  VkSemaphore *imageAcquiredSemaphore = &imageAcquiredSemaphores[currentFrame];
  VkResult acquireResult = vkAcquireNextImageKHR(
      device, swapchain, UINT64_MAX, *imageAcquiredSemaphore, VK_NULL_HANDLE,
      &swapchainImageIndex);
  if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
    resizeSwapchain();
    resized = true;
  }

  vkResetCommandPool(device, commandPools[currentFrame], 0);
}

void GpuDevice::present() {
  VkSemaphore *imageAcquiredSemaphore = &imageAcquiredSemaphores[currentFrame];
  VkSemaphore *renderCompletedSemaphore =
      &renderCompletedSemaphores[currentFrame];

  std::vector<VkSemaphoreSubmitInfo> waitSemaphores;
  waitSemaphores.reserve(2);
  waitSemaphores.emplace_back(VkSemaphoreSubmitInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .pNext = nullptr,
      .semaphore = *imageAcquiredSemaphore,
      .value = 0,
      .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      .deviceIndex = 0,
  });

  if (absoluteFrame >= FRAMES_IN_FLIGHT) {
    waitSemaphores.emplace_back(VkSemaphoreSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .semaphore = graphicsTimelineSemaphore,
        .value = absoluteFrame - FRAMES_IN_FLIGHT + 1,
        .stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        .deviceIndex = 0,
    });
  }

  std::array<VkSemaphoreSubmitInfo, 2> signalSemaphores = {
      VkSemaphoreSubmitInfo{
          .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
          .pNext = nullptr,
          .semaphore = *renderCompletedSemaphore,
          .value = 0,
          .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
          .deviceIndex = 0,
      },
      VkSemaphoreSubmitInfo{
          .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
          .pNext = nullptr,
          .semaphore = graphicsTimelineSemaphore,
          .value = absoluteFrame + 1,
          .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
          .deviceIndex = 0,
      }};

  VkCommandBufferSubmitInfo commandBufferInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
      .pNext = nullptr,
      .commandBuffer = commandBuffers[currentFrame],
      .deviceMask = 0,
  };

  VkSubmitInfo2 submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
      .pNext = nullptr,
      .flags = 0,
      .waitSemaphoreInfoCount = static_cast<uint32_t>(waitSemaphores.size()),
      .pWaitSemaphoreInfos = waitSemaphores.data(),
      .commandBufferInfoCount = 1,
      .pCommandBufferInfos = &commandBufferInfo,
      .signalSemaphoreInfoCount =
          static_cast<uint32_t>(signalSemaphores.size()),
      .pSignalSemaphoreInfos = signalSemaphores.data(),
  };

  if (vkQueueSubmit2(mainQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
    spdlog::error("Error submitting");
  }

  VkPresentInfoKHR presentInfo = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .pNext = nullptr,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = renderCompletedSemaphore,
      .swapchainCount = 1,
      .pSwapchains = &swapchain,
      .pImageIndices = &swapchainImageIndex,
      .pResults = nullptr,
  };

  VkResult result = vkQueuePresentKHR(mainQueue, &presentInfo);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
      resized) {
    resizeSwapchain();
    resized = false;
    advanceFrameCounter();
    return;
  }

  advanceFrameCounter();
}

void GpuDevice::advanceFrameCounter() {
  currentFrame = (currentFrame + 1) % FRAMES_IN_FLIGHT;
  absoluteFrame++;
}

VkCommandBuffer GpuDevice::getCommandBuffer(bool begin) {
  VkCommandBuffer cmdBuf = commandBuffers[currentFrame];

  if (begin) {
    vkResetCommandBuffer(cmdBuf, 0);

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };

    vkBeginCommandBuffer(cmdBuf, &beginInfo);
  }

  return cmdBuf;
}

Handle<Buffer> GpuDevice::createBuffer(const BufferCI &ci) {
  Handle<Buffer> handle;

  switch (ci.bufferType) {
  case BufferType::eUniform:
    handle = uniformBuffers.obtain();
    break;
  case BufferType::eStorage:
    handle = storageBuffers.obtain();
    break;
  }

  if (!handle.isValid()) {
    return handle;
  }

  handle.bufferType = ci.bufferType;

  Buffer *buffer = nullptr;
  VkDescriptorType descriptorType;
  VkBufferUsageFlags bufferUsage;
  uint32_t descriptorSetIndex;
  switch (ci.bufferType) {
  case BufferType::eUniform:
    buffer = uniformBuffers.get(handle);
    descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bufferUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    descriptorSetIndex = UNIFORM_BUFFERS_SET;
    break;
  case BufferType::eStorage:
    buffer = storageBuffers.get(handle);
    descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    descriptorSetIndex = STORAGE_BUFFERS_SET;
    break;
  }

  if (buffer == nullptr) {
    spdlog::error("Error getting buffer");
    return handle;
  }

  buffer->size = ci.size;
  buffer->name =
      std::string(ci.bufferType == BufferType::eStorage ? "Storage buffer "
                                                        : "Uniform buffer ") +
      ci.name;

  VkBufferCreateInfo bufferCI = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .size = ci.size,
      .usage = ci.usageFlags | bufferUsage,
  };

  VmaAllocationCreateInfo allocCI = {.usage = VMA_MEMORY_USAGE_AUTO};

  if (ci.mapped) {
    allocCI.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                     VMA_ALLOCATION_CREATE_MAPPED_BIT;
  }

  if (ci.readback) {
    allocCI.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                     VMA_ALLOCATION_CREATE_MAPPED_BIT;
  }

  vmaCreateBuffer(allocator, &bufferCI, &allocCI, &buffer->buffer,
                  &buffer->allocation, &buffer->allocationInfo);

  if (!buffer->name.empty()) {
    setVkObjectName(buffer->buffer, VK_OBJECT_TYPE_BUFFER, buffer->name);
  }

  if (ci.initialData) {
    if (ci.mapped) {
       memcpy(buffer->allocationInfo.pMappedData, ci.initialData, ci.size);
    } else {
      uploadBufferData(handle, ci.initialData);
    }
  }

  VkDescriptorBufferInfo descBuf = {
      .buffer = buffer->buffer,
      .offset = 0,
      .range = buffer->size,
  };

  VkWriteDescriptorSet write = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = bindlessDescriptorSets[descriptorSetIndex],
      .dstBinding = 0,
      .dstArrayElement = handle.index,
      .descriptorCount = 1,
      .descriptorType = descriptorType,
      .pBufferInfo = &descBuf,
  };

  vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

  return handle;
}

void GpuDevice::destroyBuffer(Handle<Buffer> handle) {
  if (!handle.isValid()) {
    spdlog::error("Invalid buffer handle");
    return;
  }

  Buffer *buffer = nullptr;
  switch (handle.bufferType) {
  case BufferType::eUniform:
    buffer = uniformBuffers.get(handle);
    uniformBuffers.release(handle);
    break;
  case BufferType::eStorage:
    buffer = storageBuffers.get(handle);
    storageBuffers.release(handle);
    break;
  }

  vmaDestroyBuffer(allocator, buffer->buffer, buffer->allocation);
}

void GpuDevice::resizeSwapchain() {
  vkDeviceWaitIdle(device);
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                            &surfaceCapabilities);
  swapchainExtent = surfaceCapabilities.currentExtent;

  if (swapchainExtent.width == 0 || swapchainExtent.height == 0) { // minimized
    return;
  }

  destroySwapchain();
  createSwapchain();

  destroyDrawTexture();
  createDrawTexture();
}

Handle<Texture> GpuDevice::createTexture(const TextureCI &ci) {
  Handle<Texture> handle = textures.obtain();
  if (!handle.isValid()) {
    return handle;
  }

  Texture *texture = textures.get(handle);

  texture->width = ci.width;
  texture->height = ci.height;
  texture->depth = ci.depth;
  texture->format = ci.format;
  texture->name = ci.name;

  if (ci.genMips) {
    texture->mipLevel = VkHelper::getMipLevel(ci.width, ci.height);
  } else {
    texture->mipLevel = 1;
  }
  if (ci.cubemap) {
    texture->layerCount = 6;
  } else {
    texture->layerCount = 1;
  }

  VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT;

  if (ci.format == VK_FORMAT_D32_SFLOAT) {
    usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  } else {
    usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }

  if (ci.offscreenDraw) {
    usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }

  if (ci.storage) {
    usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    Handle<uint8_t> storageTextureHandle = storageTextures.obtain();
    handle.storageIndex = storageTextureHandle.index;
  }

  VkImageCreateInfo imageCI = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = nullptr,
      .flags = static_cast<VkImageCreateFlags>(
          ci.cubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0),
      .imageType = ci.type,
      .format = ci.format,
      .extent =
          {
              .width = ci.width,
              .height = ci.height,
              .depth = ci.depth,
          },
      .mipLevels = ci.genMips ? texture->mipLevel : 1,
      .arrayLayers = static_cast<uint32_t>(ci.cubemap ? 6 : 1),
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VmaAllocationCreateInfo allocCI = {
      .usage = VMA_MEMORY_USAGE_AUTO,
  };

  vmaCreateImage(allocator, &imageCI, &allocCI, &texture->image,
                 &texture->allocation, nullptr);
  if (!ci.name.empty()) {
    setVkObjectName(texture->image, VK_OBJECT_TYPE_IMAGE, "Image " + ci.name);
  }

  VkImageViewCreateInfo imageViewCI = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .image = texture->image,
      .viewType = ci.viewType,
      .format = ci.format,
      .components = VkHelper::identityRGBA(),
      .subresourceRange = {
          .aspectMask = static_cast<VkImageAspectFlags>(
              ci.format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                : VK_IMAGE_ASPECT_COLOR_BIT),
          .baseMipLevel = 0,
          .levelCount = texture->mipLevel,
          .baseArrayLayer = 0,
          .layerCount = static_cast<uint32_t>(ci.cubemap ? 6 : 1),
      }};

  vkCreateImageView(device, &imageViewCI, nullptr, &texture->imageView);
  if (!ci.name.empty()) {
    setVkObjectName(texture->imageView, VK_OBJECT_TYPE_IMAGE_VIEW,
                    "Image View " + ci.name);
  }

  if (ci.initialData) {
    uploadTextureData(texture, ci.initialData, ci.genMips);
  }

  VkDescriptorImageInfo imageInfo = {
      .imageView = texture->imageView,
      .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
  };

  VkDescriptorImageInfo storageImageInfo = {
      .imageView = texture->imageView,
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
  };

  VkWriteDescriptorSet writes[] = {
      {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
       .dstSet = bindlessDescriptorSets[SAMPLED_IMAGES_SET],
       .dstBinding = 0,
       .dstArrayElement = handle.index,
       .descriptorCount = 1,
       .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
       .pImageInfo = &imageInfo},
      {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
       .dstSet = bindlessDescriptorSets[STORAGE_IMAGES_SET],
       .dstBinding = 0,
       .dstArrayElement = handle.storageIndex,
       .descriptorCount = 1,
       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
       .pImageInfo = &storageImageInfo},
  };

  vkUpdateDescriptorSets(device, ci.storage ? 2 : 1, writes, 0, nullptr);

  return handle;
}

void GpuDevice::destroyTexture(Handle<Texture> handle) {
  if (!handle.isValid()) {
    spdlog::error("Invalid texture handle");
    return;
  }

  Texture *texture = textures.get(handle);

  vkDestroyImageView(device, texture->imageView, nullptr);
  vmaDestroyImage(allocator, texture->image, texture->allocation);

  if (handle.storageIndex != invalidIndex) { // construct handle to release for
                                             // proper storage image indexing
    Handle<uint8_t> storageTextureHandle;
    storageTextureHandle.index = handle.storageIndex;
    storageTextures.release(storageTextureHandle);
  }
  textures.release(handle);
}

Handle<Sampler> GpuDevice::createSampler(const SamplerCI &ci) {
  Handle<Sampler> handle = samplers.obtain();
  if (!handle.isValid()) {
    return handle;
  }

  Sampler *sampler = samplers.get(handle);

  VkSamplerCreateInfo samplerCI = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .magFilter = ci.magFilter,
      .minFilter = ci.minFilter,
      .mipmapMode = ci.mipFilter,
      .addressModeU = ci.u,
      .addressModeV = ci.v,
      .addressModeW = ci.w,
      //                .mipLodBias;
      //                .anisotropyEnable;
      //                .maxAnisotropy;
      //                .compareEnable;
      //                .compareOp;
      //                .minLod;
      .maxLod = 16,
      .borderColor = ci.borderColor,
      //                .unnormalizedCoordinates;
  };

  vkCreateSampler(device, &samplerCI, nullptr, &sampler->sampler);

  VkDescriptorImageInfo samplerInfo = {
      .sampler = sampler->sampler,
  };

  VkWriteDescriptorSet write = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = bindlessDescriptorSets[SAMPLERS_SET],
      .dstBinding = 0,
      .dstArrayElement = handle.index,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
      .pImageInfo = &samplerInfo,
  };

  vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

  return handle;
}

void GpuDevice::destroySampler(Handle<Sampler> handle) {
  if (!handle.isValid()) {
    spdlog::error("Invalid sampler handle");
    return;
  }

  Sampler *sampler = samplers.get(handle);

  vkDestroySampler(device, sampler->sampler, nullptr);

  samplers.release(handle);
}

void GpuDevice::submitImmediate(VkCommandBuffer cmd) {
  vkEndCommandBuffer(cmd);

  vkResetFences(device, 1, &immediateFence);

  VkSubmitInfo submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = nullptr,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmd,
  };

  vkQueueSubmit(mainQueue, 1, &submitInfo, immediateFence);

  if (vkGetFenceStatus(device, immediateFence) != VK_SUCCESS) {
    vkWaitForFences(device, 1, &immediateFence, VK_TRUE, UINT64_MAX);
  }
}

void GpuDevice::createBindlessDescriptorSets(const GpuDeviceCreateInfo &ci) {
  std::vector<VkDescriptorPoolSize> poolSizes = {
      {
          .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = ci.bindlessSetup.uniformBuffers,
      },
      {
          .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = ci.bindlessSetup.storageBuffers,
      },
      {
          .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
          .descriptorCount = ci.bindlessSetup.sampledImages,
      },
      {
          .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .descriptorCount = ci.bindlessSetup.storageImages,
      },
      {
          .type = VK_DESCRIPTOR_TYPE_SAMPLER,
          .descriptorCount = ci.bindlessSetup.samplers,
      },
  };

  std::vector<VkDescriptorSetLayoutBinding> bindlessBindings = {
      {
          .binding = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = ci.bindlessSetup.uniformBuffers,
          .stageFlags = VK_SHADER_STAGE_ALL,
      },
      {
          .binding = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = ci.bindlessSetup.storageBuffers,
          .stageFlags = VK_SHADER_STAGE_ALL,
      },
      {
          .binding = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
          .descriptorCount = ci.bindlessSetup.sampledImages,
          .stageFlags = VK_SHADER_STAGE_ALL,
      },
      {
          .binding = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .descriptorCount = ci.bindlessSetup.storageImages,
          .stageFlags = VK_SHADER_STAGE_ALL,
      },
      {
          .binding = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
          .descriptorCount = ci.bindlessSetup.samplers,
          .stageFlags = VK_SHADER_STAGE_ALL,
      },
  };

  if (accelStructExtensionPresent && deferredHostOpExtensionPresent) {
    poolSizes.push_back({
        .type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        .descriptorCount = ci.bindlessSetup.accelStructs,
    });
    bindlessBindings.push_back({
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        .descriptorCount = ci.bindlessSetup.accelStructs,
        .stageFlags = VK_SHADER_STAGE_ALL,
    });
  }

  bindlessDescriptorSets.resize(bindlessBindings.size());
  bindlessDescriptorSetLayouts.resize(bindlessBindings.size());

  uint32_t setCount =
      ci.bindlessSetup.uniformBuffers + ci.bindlessSetup.storageBuffers +
      ci.bindlessSetup.sampledImages + ci.bindlessSetup.storageImages +
      ci.bindlessSetup.samplers + ci.bindlessSetup.accelStructs;

  VkDescriptorPoolCreateInfo poolCI = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
      .maxSets = setCount,
      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
      .pPoolSizes = poolSizes.data(),
  };

  if (vkCreateDescriptorPool(device, &poolCI, nullptr,
                             &bindlessDescriptorPool) != VK_SUCCESS) {
    spdlog::error("Failed to create bindless descriptor pool");
  }

  VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                                   VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

  VkDescriptorSetLayoutBindingFlagsCreateInfo bindlessFlags = {
      .sType =
          VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
      .pNext = nullptr,
      .bindingCount = 1,
      .pBindingFlags = &flags,
  };

  for (size_t i = 0; i < bindlessBindings.size(); i++) {
    VkDescriptorSetLayoutCreateInfo setCI = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &bindlessFlags,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = 1,
        .pBindings = &bindlessBindings[i],
    };

    if (vkCreateDescriptorSetLayout(device, &setCI, nullptr,
                                    &bindlessDescriptorSetLayouts[i]) !=
        VK_SUCCESS) {
      spdlog::error("Failed to create bindless descriptor set layout");
    }
  }

  VkDescriptorSetAllocateInfo allocInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .pNext = nullptr,
      .descriptorPool = bindlessDescriptorPool,
      .descriptorSetCount =
          static_cast<uint32_t>(bindlessDescriptorSetLayouts.size()),
      .pSetLayouts = bindlessDescriptorSetLayouts.data(),
  };

  if (vkAllocateDescriptorSets(device, &allocInfo,
                               bindlessDescriptorSets.data()) != VK_SUCCESS) {
    spdlog::error("Failed to allocate bindless descriptor sets");
  }
}

void GpuDevice::destroyBindlessDescriptorSets() {
  for (const auto &layout : bindlessDescriptorSetLayouts) {
    vkDestroyDescriptorSetLayout(device, layout, nullptr);
  }
  vkDestroyDescriptorPool(device, bindlessDescriptorPool, nullptr);
}

Buffer *GpuDevice::getBuffer(Handle<Buffer> handle) {
  switch (handle.bufferType) {
  case BufferType::eUniform:
    return uniformBuffers.get(handle);
  case BufferType::eStorage:
    return storageBuffers.get(handle);
  }
  return nullptr;
}

Texture *GpuDevice::getTexture(Handle<Texture> handle) {
  return textures.get(handle);
}

Pipeline *GpuDevice::getPipeline(Handle<Pipeline> handle) {
  return pipelines.get(handle);
}

void GpuDevice::createDefaultTextures() {
  SamplerCI defaultSamplerCI = {};
  defaultSampler = createSampler(defaultSamplerCI);

  uint32_t opaqueWhite = 0xFFFFFFFF;
  uint32_t defaultNormalColor = 0xFFFF8080;

  TextureCI ci = {
      .initialData = &opaqueWhite,
      .width = 1,
      .height = 1,
      .depth = 1,
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .type = VK_IMAGE_TYPE_2D,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
  };

  defaultTexture = createTexture(ci);

  ci.initialData = &defaultNormalColor;
  defaultNormalTexture = createTexture(ci);
}

void GpuDevice::destroyDefaultTextures() {
  destroySampler(defaultSampler);

  destroyTexture(defaultTexture);
  destroyTexture(defaultNormalTexture);
}

void GpuDevice::recreatePipeline(Handle<Pipeline> handle,
                                 const PipelineCI &ci) {
  if (!handle.isValid()) {
    spdlog::error("Invalid pipeline handle");
    return;
  }

  vkDeviceWaitIdle(device);

  Handle<Pipeline> recreatedHandle = createPipeline(ci);
  if (!recreatedHandle.isValid()) {
    spdlog::error("Failed to recreate pipeline, using old pipeline");
    return;
  }
  pipelines.swap(handle, recreatedHandle);
  destroyPipeline(recreatedHandle); // destroy old pipeline
}

void GpuDevice::uploadTextureData(Texture *texture, void *data, bool genMips) {
  size_t size = texture->width * texture->height * texture->depth * 4;

  Buffer *stagingBuffer = storageBuffers.get(stagingBufferHandle);

  memcpy(stagingBuffer->allocationInfo.pMappedData, data, size);

  VkCommandBuffer cmd = getCommandBuffer();
  VkHelper::transitionImage(cmd, texture->image, VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  VkBufferImageCopy2 region = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
      .pNext = nullptr,
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .mipLevel = 0,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
      .imageOffset = {0, 0, 0},
      .imageExtent = {texture->width, texture->height, texture->depth},
  };

  VkCopyBufferToImageInfo2 copyInfo = {
      .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
      .pNext = nullptr,
      .srcBuffer = stagingBuffer->buffer,
      .dstImage = texture->image,
      .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .regionCount = 1,
      .pRegions = &region,
  };

  vkCmdCopyBufferToImage2(cmd, &copyInfo);

  if (genMips) {
    VkHelper::genMips(cmd, texture->image, {texture->width, texture->height});
  } else {
    VkHelper::transitionImage(cmd, texture->image,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }

  submitImmediate(cmd);
}

void GpuDevice::uploadBufferData(Handle<Buffer> targetHandle, void *data) {
  if (!data) {
    return;
  }
  Buffer *stagingBuffer = getBuffer(stagingBufferHandle);
  Buffer *targetBuffer = getBuffer(targetHandle);

  memcpy(stagingBuffer->allocationInfo.pMappedData, data, targetBuffer->size);

  VkCommandBuffer cmd = getCommandBuffer();
  VkBufferCopy2 region = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
      .size = targetBuffer->size,
  };

  VkCopyBufferInfo2 copyInfo = {
      .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
      .srcBuffer = stagingBuffer->buffer,
      .dstBuffer = targetBuffer->buffer,
      .regionCount = 1,
      .pRegions = &region,
  };

  vkCmdCopyBuffer2(cmd, &copyInfo);

  VkBufferMemoryBarrier2 barrier = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
      .pNext = nullptr,
      .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      .dstAccessMask = 0,
      .buffer = targetBuffer->buffer,
      .offset = 0,
      .size = targetBuffer->size,
  };

  VkDependencyInfo depInfo = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .pNext = nullptr,
      .dependencyFlags = 0,
      .bufferMemoryBarrierCount = 1,
      .pBufferMemoryBarriers = &barrier,
  };

  vkCmdPipelineBarrier2(cmd, &depInfo);

  submitImmediate(cmd);
}

void GpuDevice::createDrawTexture() {
  TextureCI ci = {
      .width = swapchainExtent.width,
      .height = swapchainExtent.height,
      .depth = 1,
      .format = drawTextureFormat,
      .type = VK_IMAGE_TYPE_2D,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .offscreenDraw = true,
  };
  drawTexture = createTexture(ci);
}

void GpuDevice::destroyDrawTexture() { destroyTexture(drawTexture); }

void GpuDevice::transitionDrawTextureToColorAttachment(VkCommandBuffer cmd) {
  VkImageMemoryBarrier2 barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .pNext = nullptr,
      .srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
      .srcAccessMask = 0,
      .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .image = getTexture(drawTexture)->image,
      .subresourceRange = VkHelper::subresourceRange(),
  };

  VkDependencyInfo dependencyInfo = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .pNext = nullptr,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &barrier,
  };

  vkCmdPipelineBarrier2(cmd, &dependencyInfo);
}

void GpuDevice::transitionDrawTextureToTransferSrc(VkCommandBuffer cmd) {
  VkImageMemoryBarrier2 barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .pNext = nullptr,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,
      .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      .image = getTexture(drawTexture)->image,
      .subresourceRange = VkHelper::subresourceRange(),
  };

  VkDependencyInfo dependencyInfo = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .pNext = nullptr,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &barrier,
  };

  vkCmdPipelineBarrier2(cmd, &dependencyInfo);
}

void GpuDevice::transitionSwapchainTextureToTransferDst(VkCommandBuffer cmd) {
  VkImageMemoryBarrier2 barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .pNext = nullptr,
      .srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
      .srcAccessMask = 0,
      .dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,
      .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .image = swapchainImages[swapchainImageIndex],
      .subresourceRange = VkHelper::subresourceRange(),
  };

  VkDependencyInfo dependencyInfo = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .pNext = nullptr,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &barrier,
  };

  vkCmdPipelineBarrier2(cmd, &dependencyInfo);
}

void GpuDevice::transitionSwapchainTextureToPresentSrc(VkCommandBuffer cmd) {
  VkImageMemoryBarrier2 barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .pNext = nullptr,
      .srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,
      .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
      .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      .image = swapchainImages[swapchainImageIndex],
      .subresourceRange = VkHelper::subresourceRange(),
  };

  VkDependencyInfo dependencyInfo = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .pNext = nullptr,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &barrier,
  };

  vkCmdPipelineBarrier2(cmd, &dependencyInfo);
}

void GpuDevice::copyDrawTextureToSwapchain(VkCommandBuffer cmd) {
  VkHelper::copyImageToImage(cmd, getTexture(drawTexture)->image,
                             swapchainImages[swapchainImageIndex],
                             swapchainExtent, swapchainExtent);
}
} // namespace Flare
