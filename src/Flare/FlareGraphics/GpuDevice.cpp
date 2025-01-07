#include "GpuDevice.h"

#define GLFW_INCLUDE_NONE
#define VK_NO_PROTOTYPES

#include <volk.h>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <optional>
#include <algorithm>

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

        glfwWindow = gpuDeviceCI.glfwWindow;

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

        VkPhysicalDeviceFeatures2 features{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                .pNext = &descriptorIndexingFeatures,
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

        vkGetDeviceQueue(device, mainFamily, 0, &mainQueue);
        vkGetDeviceQueue(device, computeFamily, 0, &computeQueue);
        vkGetDeviceQueue(device, transferFamily, 0, &transferQueue);

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
    }

    void GpuDevice::shutdown() {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
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

        VkSwapchainCreateInfoKHR swapchainCI {
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
    }
}
