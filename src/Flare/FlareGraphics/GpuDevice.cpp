#include "GpuDevice.h"

#define GLFW_INCLUDE_NONE
#define VK_NO_PROTOTYPES
#include <volk.h>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

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

        std::vector<const char *> enabledExtensions(glfwExtensions, glfwExtensions + glfwExtensionsCount);
        std::vector<const char *> enabledLayers;

#ifdef ENABLE_VULKAN_VALIDATION
        spdlog::info("GpuDevice: Using vulkan validation layer");

        enabledExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
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

        VkDebugUtilsMessengerCreateInfoEXT debugMessengerCI {
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
                .enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size()),
                .ppEnabledExtensionNames = enabledExtensions.data(),
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

        // Physical device todo
    }

    void GpuDevice::shutdown() {
#ifdef ENABLE_VULKAN_VALIDATION
        vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
#endif
        vkDestroyInstance(instance, nullptr);
        spdlog::info("GpuDevice: Shutdown");
    }
}
