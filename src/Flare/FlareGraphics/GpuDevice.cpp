#include "GpuDevice.h"

#define GLFW_INCLUDE_NONE
#define VK_NO_PROTOTYPES
#define CGLTF_IMPLEMENTATION

#include <volk.h>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <optional>
#include <algorithm>
#include <spirv_cross.hpp>
#include <cgltf.h>
#include <stb_image.h>

#include "VkHelper.h"

namespace Flare {
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
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

        spdlog::log(log_level, "Vulkan Validation: [{}] {}", pCallbackData->pMessageIdName, pCallbackData->pMessage);

        return VK_FALSE;
    }

    void GpuDevice::init(GpuDeviceCreateInfo &gpuDeviceCI) {
        spdlog::info("GpuDevice: Initialize");

        // Set window
        glfwWindow = gpuDeviceCI.glfwWindow;

        // Init resource pools;
        pipelines.init(gpuDeviceCI.resourcePoolCI.pipelines);
        descriptorSetLayouts.init(gpuDeviceCI.resourcePoolCI.descriptorSetLayouts);
        buffers.init(gpuDeviceCI.resourcePoolCI.buffers);
        textures.init(gpuDeviceCI.resourcePoolCI.textures);
        samplers.init(gpuDeviceCI.resourcePoolCI.samplers);
        descriptorSets.init(gpuDeviceCI.resourcePoolCI.descriptorSets);

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

        std::vector<const char *> enabledInstanceExtensions(glfwExtensions, glfwExtensions + glfwExtensionsCount);
        std::vector<const char *> enabledLayers;

#ifdef ENABLE_VULKAN_VALIDATION
        spdlog::info("GpuDevice: Using vulkan validation layer");

        enabledInstanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        enabledLayers.emplace_back("VK_LAYER_KHRONOS_validation");

        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char *layerName: enabledLayers) {
            bool found = false;
            for (const auto &layerProperties: availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    found = true;
                }
            }
            if (!found) {
                spdlog::error("GpuDevice: layer {} requested but not available", layerName);
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
                .ppEnabledLayerNames = enabledLayers.empty() ? nullptr : enabledLayers.data(),
                .enabledExtensionCount = static_cast<uint32_t>(enabledInstanceExtensions.size()),
                .ppEnabledExtensionNames = enabledInstanceExtensions.data(),
        };

        if (vkCreateInstance(&instanceCI, nullptr, &instance) != VK_SUCCESS) {
            spdlog::error("GpuDevice: Failed to create Vulkan instance");
        }

        volkLoadInstance(instance);

#ifdef ENABLE_VULKAN_VALIDATION
        if (vkCreateDebugUtilsMessengerEXT(instance, &debugMessengerCI, nullptr, &debugMessenger) != VK_SUCCESS) {
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
        if (vkEnumeratePhysicalDevices(instance, &gpuCount, physicalDevices.data()) != VK_SUCCESS) {
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
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);
        spdlog::info("GpuDevice: Using {}", physicalDeviceProperties.deviceName);

        // queues, separate family for each queue, todo: same family for queues in case can't find separate family
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

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
                ((family.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0) && family.queueFlags & VK_QUEUE_TRANSFER_BIT) {
                transferFamilyOpt = fi;
                continue;
            }

            if (mainFamilyOpt.has_value() && computeFamilyOpt.has_value() && transferFamilyOpt.has_value()) {
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

        std::vector<VkDeviceQueueCreateInfo> queueCI = {mainQueueCI, computeQueueCI, transferQueueCI};

        // device extensions
        uint32_t deviceExtensionCount = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, nullptr);
        std::vector<VkExtensionProperties> deviceExtensions(deviceExtensionCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, deviceExtensions.data());

        std::vector<const char *> enabledDeviceExtensions;

        bool swapchainExtensionPresent = false;

        for (size_t i = 0; i < deviceExtensionCount; i++) {
            if (strcmp(deviceExtensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                swapchainExtensionPresent = true;
            }
        }

        if (!swapchainExtensionPresent) {
            spdlog::error("GpuDevice: Swapchain extension not present");
        }

        enabledDeviceExtensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        // device features
        VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
                .pNext = nullptr
        };

        VkPhysicalDeviceSynchronization2Features synchronization2Features{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
                .pNext = &dynamicRenderingFeatures,
        };

        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
                .pNext = &synchronization2Features,
        };

        VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
                .pNext = &bufferDeviceAddressFeatures,
        };

        VkPhysicalDeviceTimelineSemaphoreFeatures timelineSemaphoreFeatures{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
                .pNext = &descriptorIndexingFeatures,
        };

        VkPhysicalDeviceFeatures2 features{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                .pNext = &timelineSemaphoreFeatures,
        };

        vkGetPhysicalDeviceFeatures2(physicalDevice, &features);

        // logical device creation
        VkDeviceCreateInfo deviceCI{
                .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .pNext = &features,
                .flags = 0,
                .queueCreateInfoCount = static_cast<uint32_t>(queueCI.size()),
                .pQueueCreateInfos = queueCI.data(),
                .enabledExtensionCount = static_cast<uint32_t>(enabledDeviceExtensions.size()),
                .ppEnabledExtensionNames = enabledDeviceExtensions.data(),
        };

        if (vkCreateDevice(physicalDevice, &deviceCI, nullptr, &device) != VK_SUCCESS) {
            spdlog::error("GpuDevice: Failed to create logical device");
        }

        volkLoadDevice(device);

        vkGetDeviceQueue(device, mainFamily, 0, &mainQueue);
        vkGetDeviceQueue(device, computeFamily, 0, &computeQueue);
        vkGetDeviceQueue(device, transferFamily, 0, &transferQueue);

        // vma
        VmaVulkanFunctions vmaFunctions = {
                .vkGetPhysicalDeviceProperties       = vkGetPhysicalDeviceProperties,
                .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
                .vkAllocateMemory                    = vkAllocateMemory,
                .vkFreeMemory                        = vkFreeMemory,
                .vkMapMemory                         = vkMapMemory,
                .vkUnmapMemory                       = vkUnmapMemory,
                .vkFlushMappedMemoryRanges           = vkFlushMappedMemoryRanges,
                .vkInvalidateMappedMemoryRanges      = vkInvalidateMappedMemoryRanges,
                .vkBindBufferMemory                  = vkBindBufferMemory,
                .vkBindImageMemory                   = vkBindImageMemory,
                .vkGetBufferMemoryRequirements       = vkGetBufferMemoryRequirements,
                .vkGetImageMemoryRequirements        = vkGetImageMemoryRequirements,
                .vkCreateBuffer                      = vkCreateBuffer,
                .vkDestroyBuffer                     = vkDestroyBuffer,
                .vkCreateImage                       = vkCreateImage,
                .vkDestroyImage                      = vkDestroyImage,
                .vkCmdCopyBuffer                     = vkCmdCopyBuffer,
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

        // surface and swapchain
        if (glfwCreateWindowSurface(instance, glfwWindow, nullptr, &surface) != VK_SUCCESS) {
            spdlog::error("GpuDevice: Failed to create window surface");
        }

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);

        uint32_t surfaceFormatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr);
        if (surfaceFormatCount != 0) {
            surfaceFormats.resize(surfaceFormatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());
        }

        if (surfaceFormats.empty() || presentModes.empty()) {
            spdlog::error("GpuDevice: Swapchain not supported");
        }

        setSurfaceFormat(VK_FORMAT_B8G8R8A8_SRGB);
        setPresentMode(VK_PRESENT_MODE_MAILBOX_KHR);
        setSwapchainExtent();
        createSwapchain();

        VkFenceCreateInfo fenceCI = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        };

        vkCreateFence(device, &fenceCI, nullptr, &immediateFence);

        VkSemaphoreCreateInfo semaphoreCI = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
        };

        // we need these per-frame semaphores because vulkan wsi swapchain doesn't support timeline semaphores
        for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
            vkCreateSemaphore(device, &semaphoreCI, nullptr, &imageAcquiredSemaphores[i]);
            vkCreateSemaphore(device, &semaphoreCI, nullptr, &renderCompletedSemaphores[i]);
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
            if (vkCreateCommandPool(device, &commandPoolCI, nullptr, &commandPools[i]) != VK_SUCCESS) {
                spdlog::error("Failed to create command pool");
            }

            commandAllocInfo.commandPool = commandPools[i];

            if (vkAllocateCommandBuffers(device, &commandAllocInfo, &commandBuffers[i]) != VK_SUCCESS) {
                spdlog::error("Failed to allocated command buffer");
            }
        }

        SamplerCI defaultSamplerCI = {};
        defaultSampler = createSampler(defaultSamplerCI);

        uint32_t opaqueWhite = 0xFFFFFFFF;
        TextureCI defaultTextureCI = {
                .initialData = &opaqueWhite,
                .width = 1,
                .height = 1,
                .depth = 1,
                .mipCount = 1,
                .layerCount = 1,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .type = VK_IMAGE_TYPE_2D,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
        };
        defaultTexture = createTexture(defaultTextureCI);

        const uint32_t globalPoolElements = 128;
        std::vector<VkDescriptorPoolSize> poolSizes = {
                {VK_DESCRIPTOR_TYPE_SAMPLER,                globalPoolElements},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, globalPoolElements},
                {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          globalPoolElements},
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          globalPoolElements},
                {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   globalPoolElements},
                {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   globalPoolElements},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         globalPoolElements},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         globalPoolElements},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, globalPoolElements},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, globalPoolElements},
                {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       globalPoolElements}
        };

        VkDescriptorPoolCreateInfo poolCI = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .maxSets = gpuDeviceCI.resourcePoolCI.descriptorSets,
                .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                .pPoolSizes = poolSizes.data(),
        };

        if (vkCreateDescriptorPool(device, &poolCI, nullptr, &descriptorPool) != VK_SUCCESS) {
            spdlog::error("Failed to create descriptor pool");
        }

        std::vector<VkDescriptorPoolSize> bindlessPoolSizes = {
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_BINDLESS_RESOURCES},
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          MAX_BINDLESS_RESOURCES},
        };

        VkDescriptorPoolCreateInfo bindlessPoolCI = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .pNext = nullptr,
                .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
                .maxSets = static_cast<uint32_t>(MAX_BINDLESS_RESOURCES * bindlessPoolSizes.size()),
                .poolSizeCount = static_cast<uint32_t>(bindlessPoolSizes.size()),
                .pPoolSizes = bindlessPoolSizes.data(),
        };

        if (vkCreateDescriptorPool(device, &bindlessPoolCI, nullptr, &bindlessDescriptorPool) != VK_SUCCESS) {
            spdlog::error("Failed to create bindless descriptor pool");
        }

        std::array<VkDescriptorSetLayoutBinding, 2> bindlessBindings = {
                VkDescriptorSetLayoutBinding{
                        .binding = BINDLESS_TEXTURE_BINDING,
                        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .descriptorCount = MAX_BINDLESS_RESOURCES,
                        .stageFlags = VK_SHADER_STAGE_ALL,
                        .pImmutableSamplers = nullptr,
                },
                VkDescriptorSetLayoutBinding{
                        .binding = BINDLESS_TEXTURE_BINDING + 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                        .descriptorCount = MAX_BINDLESS_RESOURCES,
                        .stageFlags = VK_SHADER_STAGE_ALL,
                        .pImmutableSamplers = nullptr,
                },
        };

        VkDescriptorBindingFlags bindlessFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                                                 VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
        VkDescriptorBindingFlags bindlessFlagsArray[2] = {bindlessFlags, bindlessFlags};

        VkDescriptorSetLayoutBindingFlagsCreateInfo bindlessBindingFlags = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .pNext = nullptr,
                .bindingCount = bindlessBindings.size(),
                .pBindingFlags = bindlessFlagsArray,
        };

        DescriptorSetLayoutCI bindlessLayoutCI = {
                .bindings = bindlessBindings.data(),
                .bindingCount = bindlessBindings.size(),
                .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                .bindingFlags = &bindlessBindingFlags,
        };

        bindlessDescriptorSetLayoutHandle = createDescriptorSetLayout(bindlessLayoutCI);

        DescriptorSetCI bindlessSetCI = {
                .layout = bindlessDescriptorSetLayoutHandle,
                .bindless = true,
        };

        bindlessDescriptorSetHandle = createDescriptorSet(bindlessSetCI);
    }

    void GpuDevice::shutdown() {
        vkDeviceWaitIdle(device);

        destroySampler(defaultSampler);
        destroyTexture(defaultTexture);

        destroyDescriptorSetLayout(bindlessDescriptorSetLayoutHandle);

        vkDestroyDescriptorPool(device, bindlessDescriptorPool, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);

        for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
            vkDestroyCommandPool(device, commandPools[i], nullptr);
        }

        vkDestroyFence(device, immediateFence, nullptr);
        vkDestroySemaphore(device, graphicsTimelineSemaphore, nullptr);
        for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device, imageAcquiredSemaphores[i], nullptr);
            vkDestroySemaphore(device, renderCompletedSemaphores[i], nullptr);
        }

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

        for (const auto &availableFormat: surfaceFormats) {
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
        for (const auto &availableMode: presentModes) {
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

        if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max() &&
            surfaceCapabilities.currentExtent.height != std::numeric_limits<uint32_t>::max()) {
            swapchainExtent = surfaceCapabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(glfwWindow, &width, &height);

            VkExtent2D actualExtent = {
                    static_cast<uint32_t>(width),
                    static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width,
                                            surfaceCapabilities.minImageExtent.width,
                                            surfaceCapabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height,
                                             surfaceCapabilities.minImageExtent.height,
                                             surfaceCapabilities.maxImageExtent.height);

            swapchainExtent = actualExtent;
        }
    }

    void GpuDevice::createSwapchain() {
        uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
        if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount) {
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
                .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices = nullptr,
                .preTransform = surfaceCapabilities.currentTransform,
                .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                .presentMode = presentMode,
                .clipped = VK_TRUE,
                .oldSwapchain = VK_NULL_HANDLE,
        };

        if (vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &swapchain) != VK_SUCCESS) {
            spdlog::error("GpuDevice: Failed to create swapchain");
        }

        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
        depthTextures.resize(imageCount);
        swapchainImageViews.resize(imageCount);
        swapchainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

        TextureCI depthTextureCI = {
                .width = static_cast<uint16_t>(swapchainExtent.width),
                .height = static_cast<uint16_t>(swapchainExtent.height),
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

            if (vkCreateImageView(device, &imageViewCI, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
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
        Handle<Pipeline> handle = pipelines.obtain();
        if (!handle.isValid()) {
            return handle;
        }

        Pipeline *pipeline = pipelines.get(handle);
        ReflectOutput reflectOutput;

        std::vector<VkShaderModule> modules;
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

        for (const auto &shaderBinary: ci.shaderStages.shaderBinaries) {
            reflect(reflectOutput, shaderBinary.spirv, shaderBinary.execModels);

            VkShaderModuleCreateInfo shaderModuleCI{
                    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = 0,
                    .codeSize = shaderBinary.spirv.size() * sizeof(uint32_t),
                    .pCode = shaderBinary.spirv.data(),
            };

            VkShaderModule shaderModule;
            if (vkCreateShaderModule(device, &shaderModuleCI, nullptr, &shaderModule) != VK_SUCCESS) {
                spdlog::error("Failed to create shader module");
            }
            modules.push_back(shaderModule);

            for (const auto &execModel: shaderBinary.execModels) {
                shaderStages.emplace_back(VkPipelineShaderStageCreateInfo{
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .pNext = nullptr,
                        .flags = 0,
                        .stage = execModel.stage,
                        .module = shaderModule,
                        .pName = execModel.entryPointName,
                        .pSpecializationInfo = nullptr,
                });
            }
        }

        std::vector<VkDescriptorSetLayout> pipelineSetLayouts;
        pipelineSetLayouts.reserve(reflectOutput.getSetCount());

        for (auto &set: reflectOutput.descriptorSets) {
            uint32_t setIndex = set.first;
            if (setIndex == BINDLESS_SET) {
                pipelineSetLayouts.push_back(
                        descriptorSetLayouts.get(bindlessDescriptorSetLayoutHandle)->descriptorSetLayout);
                continue;
            }

            std::vector<VkDescriptorSetLayoutBinding> &bindings = set.second;

            DescriptorSetLayoutCI descriptorSetLayoutCI = {
                    .bindings = bindings.data(),
                    .bindingCount = bindings.size(),
            };

            Handle<DescriptorSetLayout> descriptorSetLayoutHandle = createDescriptorSetLayout(
                    descriptorSetLayoutCI);

            pipeline->descriptorSetLayoutHandles.push_back(descriptorSetLayoutHandle);
            pipelineSetLayouts.push_back(descriptorSetLayouts.get(descriptorSetLayoutHandle)->descriptorSetLayout);
        }

        VkPipelineLayoutCreateInfo pipelineLayoutCI = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .setLayoutCount = static_cast<uint32_t>(pipelineSetLayouts.size()),
                .pSetLayouts = pipelineSetLayouts.data(),
                .pushConstantRangeCount = 0, // TODO: push constants reflection
                .pPushConstantRanges = nullptr,
        };

        if (vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipeline->pipelineLayout) != VK_SUCCESS) {
            spdlog::error("Failed to create pipeline layout");
        }

        std::vector<VkDynamicState> dynamicStates = {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR,
        };
        VkPipelineDynamicStateCreateInfo dynamicStateCI = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                .pDynamicStates = dynamicStates.data()
        };

        VkPipelineVertexInputStateCreateInfo vertexInputCI = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .vertexBindingDescriptionCount = static_cast<uint32_t>(ci.vertexInput.vertexBindings.size()),
                .pVertexBindingDescriptions = ci.vertexInput.vertexBindings.data(),
                .vertexAttributeDescriptionCount = static_cast<uint32_t>(ci.vertexInput.vertexAttributes.size()),
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
                .depthBiasEnable = VK_FALSE,
                .depthBiasConstantFactor = 0.f,
                .depthBiasClamp = 0.f,
                .depthBiasSlopeFactor = 0.f,
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
                .depthWriteEnable = ci.depthStencil.depthWriteEnable ? VK_TRUE : VK_FALSE,
                .depthCompareOp = ci.depthStencil.depthCompareOp,
                .depthBoundsTestEnable = VK_FALSE,
                .stencilTestEnable = ci.depthStencil.stencilTestEnable ? VK_TRUE : VK_FALSE,
                .front = ci.depthStencil.front,
                .back = ci.depthStencil.back,
                .minDepthBounds = 0.f,
                .maxDepthBounds = 1.f,
        };

        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
        if (ci.colorBlend.attachments.empty()) {
            colorBlendAttachments.emplace_back(
                    VkPipelineColorBlendAttachmentState{
                            .blendEnable = VK_FALSE,
                            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                              VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT |
                                              VK_COLOR_COMPONENT_A_BIT,
                    }
            );
        } else {
            colorBlendAttachments.reserve(ci.colorBlend.attachments.size());
            for (const auto &attachment: ci.colorBlend.attachments) {
                colorBlendAttachments.emplace_back(
                        VkPipelineColorBlendAttachmentState{
                                .blendEnable = attachment.enable ? VK_TRUE : VK_FALSE,
                                .srcColorBlendFactor = attachment.srcColor,
                                .dstColorBlendFactor = attachment.dstColor,
                                .colorBlendOp = attachment.colorOp,
                                .srcAlphaBlendFactor = attachment.srcAlpha,
                                .dstAlphaBlendFactor = attachment.dstAlpha,
                                .alphaBlendOp = attachment.alphaOp,
                                .colorWriteMask = attachment.colorWriteMask,
                        }
                );
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
                .colorAttachmentCount = static_cast<uint32_t>(ci.rendering.colorFormats.size()),
                .pColorAttachmentFormats = ci.rendering.colorFormats.empty() ? nullptr
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

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, // todo: pipeline cache
                                      1, &pipelineCI, nullptr, &pipeline->pipeline) != VK_SUCCESS) {
            spdlog::error("Failed to create graphics pipeline");
        }
        pipeline->bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // todo: compute

        for (const auto &module: modules) {
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

        for (const auto &descSetHandle: pipeline->descriptorSetLayoutHandles) {
            destroyDescriptorSetLayout(descSetHandle);
        }

        vkDestroyPipeline(device, pipeline->pipeline, nullptr);

        pipelines.release(handle);
    }

    void GpuDevice::reflect(ReflectOutput &reflection, const std::vector<uint32_t> &spirv,
                            const std::span<ShaderExecModel> &execModels) const {
        using namespace spirv_cross;

        Compiler comp(spirv);
        auto entryPoints = comp.get_entry_points_and_stages();
        const char *stageString;

        for (const auto &model: execModels) {
            spv::ExecutionModel spvExecModel = spv::ExecutionModelMax;
            switch (model.stage) {
                case VK_SHADER_STAGE_VERTEX_BIT:
                    spvExecModel = spv::ExecutionModelVertex;
                    stageString = "Vertex";
                    break;
                case VK_SHADER_STAGE_FRAGMENT_BIT:
                    spvExecModel = spv::ExecutionModelFragment;
                    stageString = "Fragment";
                    break;
                case VK_SHADER_STAGE_COMPUTE_BIT:
                    spvExecModel = spv::ExecutionModelGLCompute;
                    stageString = "Compute";
                    break;
                default:
                    spdlog::error("Invalid execution model");
                    break;
            }

            comp.set_entry_point(model.entryPointName, spvExecModel);

            ShaderResources res = comp.get_shader_resources();
            for (auto &uniform: res.uniform_buffers) {
                uint32_t set = comp.get_decoration(uniform.id, spv::DecorationDescriptorSet);
                uint32_t binding = comp.get_decoration(uniform.id, spv::DecorationBinding);

                spdlog::info("{} shader: Found UBO {} at set = {}, binding = {}",
                             stageString, uniform.name, set, binding);
                reflection.addBinding(set, binding, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, model.stage);
            }

            for (auto &image: res.separate_images) {
                uint32_t set = comp.get_decoration(image.id, spv::DecorationDescriptorSet);
                uint32_t binding = comp.get_decoration(image.id, spv::DecorationBinding);

                spdlog::info("{} shader: Found sampled image {} at set = {}, binding = {}",
                             stageString, image.name, set, binding);
                reflection.addBinding(set, binding, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, model.stage);
            }

            for (auto &sampler: res.separate_samplers) {
                uint32_t set = comp.get_decoration(sampler.id, spv::DecorationDescriptorSet);
                uint32_t binding = comp.get_decoration(sampler.id, spv::DecorationBinding);

                spdlog::info("{} shader: Found sampler {} at set = {}, binding = {}",
                             stageString, sampler.name, set, binding);
                reflection.addBinding(set, binding, 1, VK_DESCRIPTOR_TYPE_SAMPLER, model.stage);
            }

            for (auto &sampledImage: res.sampled_images) {
                uint32_t set = comp.get_decoration(sampledImage.id, spv::DecorationDescriptorSet);

                uint32_t binding = comp.get_decoration(sampledImage.id, spv::DecorationBinding);
                spdlog::info("{} shader: Found combined image sampler {} at set = {}, binding = {}",
                             stageString, sampledImage.name, set, binding);
                reflection.addBinding(set, binding, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, model.stage);
            }

            for (auto &storageBuffer: res.storage_buffers) {
                uint32_t set = comp.get_decoration(storageBuffer.id, spv::DecorationDescriptorSet);

                uint32_t binding = comp.get_decoration(storageBuffer.id, spv::DecorationBinding);
                spdlog::info("{} shader: Found SSBO {} at set = {}, binding = {}",
                             stageString, storageBuffer.name, set, binding);
                reflection.addBinding(set, binding, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, model.stage);
            }
        }
    }

    Handle<DescriptorSetLayout> GpuDevice::createDescriptorSetLayout(const DescriptorSetLayoutCI &ci) {
        Handle<DescriptorSetLayout> handle = descriptorSetLayouts.obtain();
        if (!handle.isValid()) {
            return handle;
        }

        DescriptorSetLayout *layout = descriptorSetLayouts.get(handle);

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = ci.bindingFlags,
                .flags = ci.flags,
                .bindingCount = static_cast<uint32_t>(ci.bindingCount),
                .pBindings = ci.bindings,
        };

        if (vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr,
                                        &layout->descriptorSetLayout) != VK_SUCCESS) {
            spdlog::error("Failed to create descriptor set layout");
        }

        for (size_t i = 0; i < ci.bindingCount; i++) {
            assert(!layout->bindings.contains(ci.bindings[i].binding));
            layout->bindings.insert({ci.bindings[i].binding, ci.bindings[i]});
        }

        return handle;
    }

    void GpuDevice::destroyDescriptorSetLayout(Handle<DescriptorSetLayout> handle) {
        if (!handle.isValid()) {
            spdlog::error("Invalid descriptor set layout handle");
            return;
        }

        DescriptorSetLayout *layout = descriptorSetLayouts.get(handle);
        vkDestroyDescriptorSetLayout(device, layout->descriptorSetLayout, nullptr);

        descriptorSetLayouts.release(handle);
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
        VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                                       *imageAcquiredSemaphore, VK_NULL_HANDLE,
                                                       &swapchainImageIndex);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
            resizeSwapchain();
            resized = true;
        }

        vkResetCommandPool(device, commandPools[currentFrame], 0);
    }

    void GpuDevice::present() {
        VkSemaphore *imageAcquiredSemaphore = &imageAcquiredSemaphores[currentFrame];
        VkSemaphore *renderCompletedSemaphore = &renderCompletedSemaphores[currentFrame];

        if (!bindlessTextureUpdates.empty()) {
            VkWriteDescriptorSet bindlessWrites[MAX_BINDLESS_RESOURCES];
            VkDescriptorImageInfo bindlessImageInfo[MAX_BINDLESS_RESOURCES];
            uint32_t writeIndex = 0;

            for (auto &handle: bindlessTextureUpdates) {
                Texture *texture = textures.get(handle);

                VkDescriptorImageInfo &imageInfo = bindlessImageInfo[writeIndex];
                imageInfo = {
                        .sampler = texture->sampler.isValid() ? samplers.get(texture->sampler)->sampler :
                                   samplers.get(defaultSampler)->sampler,
                        .imageView = texture->format != VK_FORMAT_UNDEFINED ? texture->imageView :
                                     textures.get(defaultTexture)->imageView,
                        .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                };

                VkWriteDescriptorSet &descriptorWrite = bindlessWrites[writeIndex];
                descriptorWrite = {
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .pNext = nullptr,
                        .dstSet = descriptorSets.get(bindlessDescriptorSetHandle)->descriptorSet,
                        .dstBinding = BINDLESS_TEXTURE_BINDING,
                        .dstArrayElement = handle.index,
                        .descriptorCount = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .pImageInfo = &imageInfo,
                };

                writeIndex++;
            }

            bindlessTextureUpdates.clear();

            vkUpdateDescriptorSets(device, writeIndex, bindlessWrites, 0, nullptr);
        }

        std::vector<VkSemaphoreSubmitInfo> waitSemaphores;
        waitSemaphores.reserve(2);
        waitSemaphores.emplace_back(
                VkSemaphoreSubmitInfo{
                        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                        .pNext = nullptr,
                        .semaphore = *imageAcquiredSemaphore,
                        .value = 0,
                        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        .deviceIndex = 0,
                }
        );

        if (absoluteFrame >= FRAMES_IN_FLIGHT) {
            waitSemaphores.emplace_back(
                    VkSemaphoreSubmitInfo{
                            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                            .pNext = nullptr,
                            .semaphore = graphicsTimelineSemaphore,
                            .value = absoluteFrame - FRAMES_IN_FLIGHT + 1,
                            .stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                            .deviceIndex = 0,
                    }
            );
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
                }
        };

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
                .signalSemaphoreInfoCount = static_cast<uint32_t>(signalSemaphores.size()),
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
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || resized) {
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
        Handle<Buffer> handle = buffers.obtain();
        if (!handle.isValid()) {
            return handle;
        }

        Buffer *buffer = buffers.get(handle);

        VkBufferCreateInfo bufferCI = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .size = ci.size,
                .usage = ci.usageFlags,
        };

        VmaAllocationCreateInfo allocCI = {.usage = VMA_MEMORY_USAGE_AUTO};

        if (ci.mapped) {
            allocCI.flags =
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }

        vmaCreateBuffer(allocator, &bufferCI, &allocCI, &buffer->buffer, &buffer->allocation,
                        &buffer->allocationInfo);

        buffer->size = ci.size;

        return handle;
    }

    void GpuDevice::destroyBuffer(Handle<Buffer> handle) {
        if (!handle.isValid()) {
            spdlog::error("Invalid buffer handle");
            return;
        }

        Buffer *buffer = buffers.get(handle);

        vmaDestroyBuffer(allocator, buffer->buffer, buffer->allocation);

        buffers.release(handle);
    }

    void GpuDevice::resizeSwapchain() {
        vkDeviceWaitIdle(device);
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
        swapchainExtent = surfaceCapabilities.currentExtent;

        if (swapchainExtent.width == 0 || swapchainExtent.height == 0) { // minimized
            return;
        }

        destroySwapchain();
        createSwapchain();
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

        VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        if (ci.format == VK_FORMAT_D32_SFLOAT) {
            usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        } else {
            usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }

        VkImageCreateInfo imageCI = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .imageType = ci.type,
                .format = ci.format,
                .extent = {
                        .width = ci.width,
                        .height = ci.height,
                        .depth = ci.depth,
                },
                .mipLevels = ci.mipCount,
                .arrayLayers = ci.layerCount,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = usage,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        VmaAllocationCreateInfo allocCI = {
                .usage = VMA_MEMORY_USAGE_AUTO,
        };

        vmaCreateImage(allocator, &imageCI, &allocCI, &texture->image, &texture->allocation, nullptr);

        VkImageViewCreateInfo imageViewCI = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .image = texture->image,
                .viewType = ci.viewType,
                .format = ci.format,
                .components = VkHelper::identityRGBA(),
                .subresourceRange = VkHelper::subresourceRange(ci.format == VK_FORMAT_D32_SFLOAT),
        };

        vkCreateImageView(device, &imageViewCI, nullptr, &texture->imageView);

        if (ci.initialData) {
            size_t dataSize = ci.width * ci.height * ci.depth * 4;

            BufferCI stagingBufferCI = {
                    .size = dataSize,
                    .usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    .mapped = true,
            };
            Handle<Buffer> textureStagingBufferHandle = createBuffer(stagingBufferCI);
            Buffer *textureStagingBuffer = buffers.get(textureStagingBufferHandle);

            memcpy(textureStagingBuffer->allocationInfo.pMappedData, ci.initialData, dataSize);

            VkCommandBuffer cmd = getCommandBuffer();
            VkHelper::transitionImage(cmd, texture->image, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VkBufferImageCopy2 region = {
                    .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                    .pNext = nullptr,
                    .bufferOffset = 0,
                    .bufferRowLength = 0,
                    .bufferImageHeight = 0,
                    .imageSubresource = {
                            .aspectMask = static_cast<VkImageAspectFlags>(ci.format == VK_FORMAT_D32_SFLOAT ?
                                                                          VK_IMAGE_ASPECT_DEPTH_BIT :
                                                                          VK_IMAGE_ASPECT_COLOR_BIT),
                            .mipLevel = 0,
                            .baseArrayLayer = 0,
                            .layerCount = 1,
                    },
                    .imageOffset = {0, 0, 0},
                    .imageExtent = {ci.width, ci.height, ci.depth},
            };

            VkCopyBufferToImageInfo2 copyInfo = {
                    .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
                    .pNext = nullptr,
                    .srcBuffer = textureStagingBuffer->buffer,
                    .dstImage = texture->image,
                    .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .regionCount = 1,
                    .pRegions = &region,
            };

            vkCmdCopyBufferToImage2(cmd, &copyInfo);

            VkHelper::transitionImage(cmd, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            submitImmediate(cmd);

            // todo: mips

            // todo: keep a persistently mapped staging buffer, and recreate it if needed?
            destroyBuffer(textureStagingBufferHandle);
        }

        bindlessTextureUpdates.push_back(handle);

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

        textures.release(handle);
    }

    Handle<DescriptorSet> GpuDevice::createDescriptorSet(const DescriptorSetCI &ci) {
        Handle<DescriptorSet> handle = descriptorSets.obtain();
        if (!handle.isValid()) {
            return handle;
        }

        DescriptorSet *descriptorSet = descriptorSets.get(handle);

        DescriptorSetLayout *layout = descriptorSetLayouts.get(ci.layout);

        VkDescriptorSetAllocateInfo allocInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext = nullptr,
                .descriptorPool = ci.bindless ? bindlessDescriptorPool : descriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &layout->descriptorSetLayout,
        };

        if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet->descriptorSet) != VK_SUCCESS) {
            spdlog::error("Failed to allocate descriptors set");
        }

        std::vector<VkWriteDescriptorSet> writeDescSets(ci.resourceCount);
        std::vector<VkDescriptorBufferInfo> bufferInfos(ci.resourceCount);
        std::vector<VkDescriptorImageInfo> imageInfos(ci.resourceCount);

        for (size_t i = 0; i < ci.resourceCount; i++) {
            VkDescriptorSetLayoutBinding layoutBinding = layout->bindings.at(ci.bindings[i]);

            switch (layoutBinding.descriptorType) {
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: {
                    Handle<Buffer> bufferHandle = ci.buffers[i];
                    Buffer *buffer = buffers.get(bufferHandle);

                    bufferInfos[i] = {
                            .buffer = buffer->buffer,
                            .offset = 0,
                            .range = buffer->size,
                    };

                    writeDescSets[i] = {
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .pNext = nullptr,
                            .dstSet = descriptorSet->descriptorSet,
                            .dstBinding = layoutBinding.binding,
                            .dstArrayElement = 0,
                            .descriptorCount = 1,
                            .descriptorType = layoutBinding.descriptorType,
                            .pImageInfo = nullptr,
                            .pBufferInfo = &bufferInfos[i],
                            .pTexelBufferView = nullptr,
                    };
                    break;
                }
                case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: {
                    Handle<Texture> textureHandle = ci.textures[i];
                    Texture *texture = textures.get(textureHandle);

                    imageInfos[i] = {
                            .sampler = nullptr,
                            .imageView = texture->imageView,
                            .imageLayout = texture->format == VK_FORMAT_D32_SFLOAT ?
                                           VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL :
                                           VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                    };

                    writeDescSets[i] = {
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .pNext = nullptr,
                            .dstSet = descriptorSet->descriptorSet,
                            .dstBinding = layoutBinding.binding,
                            .dstArrayElement = 0,
                            .descriptorCount = 1,
                            .descriptorType = layoutBinding.descriptorType,
                            .pImageInfo = &imageInfos[i],
                            .pBufferInfo = nullptr,
                            .pTexelBufferView = nullptr,
                    };
                    break;
                }
                case VK_DESCRIPTOR_TYPE_SAMPLER: {
                    Handle<Sampler> samplerHandle = ci.samplers[i];
                    Sampler *sampler = samplers.get(samplerHandle);

                    imageInfos[i] = {
                            .sampler = sampler->sampler,
                    };

                    writeDescSets[i] = {
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .pNext = nullptr,
                            .dstSet = descriptorSet->descriptorSet,
                            .dstBinding = layoutBinding.binding,
                            .dstArrayElement = 0,
                            .descriptorCount = 1,
                            .descriptorType = layoutBinding.descriptorType,
                            .pImageInfo = &imageInfos[i],
                            .pBufferInfo = nullptr,
                            .pTexelBufferView = nullptr,
                    };
                    break;
                }
                default:
                    break;
            }
        }

        vkUpdateDescriptorSets(device, ci.resourceCount, writeDescSets.data(), 0, nullptr);

        return handle;
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
//                .maxLod;
//                .borderColor;
//                .unnormalizedCoordinates;
        };

        vkCreateSampler(device, &samplerCI, nullptr, &sampler->sampler);

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
}
