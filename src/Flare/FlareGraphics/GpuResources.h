#pragma once

#include <volk.h>
#include <cstdint>
#include <cassert>
#include <vector>
#include <span>
#include <spdlog/spdlog.h>
#include <vk_mem_alloc.h>
#include <map>
#include <glm/glm.hpp>
#include <filesystem>

namespace Flare {
    constexpr uint32_t invalidIndex = 0xFFFFFFFF;

    template<typename T>
    struct Handle {
        uint32_t index = invalidIndex;

        bool isValid() const { return index != invalidIndex; }

        void invalidate() { index = invalidIndex; }
    };

    template<typename T>
    struct ResourcePool {
        std::vector<T> data;
        std::vector<uint32_t> availableHandles;

        size_t poolSize = 0;
        size_t freeHandleHead = 0;
        size_t usedHandleCount = 0;

        void init(size_t size) {
            poolSize = size;
            data.resize(poolSize);
            availableHandles.resize(poolSize);
            for (size_t i = 0; i < poolSize; i++) {
                availableHandles[i] = i;
            }
        }

        Handle<T> obtain() {
            if (freeHandleHead < poolSize) {
                uint32_t handleIndex = availableHandles[freeHandleHead++];
                usedHandleCount++;
                return Handle<T>{.index = handleIndex};
            } else {
                spdlog::error("ResourcePool: No more resources left");
                return Handle<T>{.index = invalidIndex};
            }
        }

        void release(Handle<T> handle) {
            if (handle.isValid() && handle.index < poolSize) {
                availableHandles[--freeHandleHead] = handle.index;
                usedHandleCount--;
            } else {
                spdlog::error("ResourcePool: Attempting to release an invalid handle");
            }
        }

        void swap(Handle<T> l, Handle<T> r) {
            if (!l.isValid() || !r.isValid()) {
                spdlog::error("ResourcePool: Attempting to swap invalid handles");
                return;
            }
            std::swap(data[l.index], data[r.index]);
        }

        T *get(Handle<T> handle) {
            if (handle.isValid() && handle.index < poolSize) {
                return &data[handle.index];
            } else {
                spdlog::error("ResourcePool: Invalid handle access");
                return nullptr;
            }
        }
    };

    struct ShaderStage {
        std::filesystem::path path;
        VkShaderStageFlagBits stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
    };

    struct VertexInputCI {
        std::vector<VkVertexInputBindingDescription> vertexBindings;
        std::vector<VkVertexInputAttributeDescription> vertexAttributes;

        VertexInputCI &addBinding(VkVertexInputBindingDescription binding);

        VertexInputCI &addAttribute(VkVertexInputAttributeDescription attribute);
    };

    struct RasterizationCI {
        VkCullModeFlags cullMode = VK_CULL_MODE_NONE;
        VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        bool depthBiasEnable = false;
        float depthBiasConstant = 0.f;
        float depthBiasSlope = 0.f;
    };

    struct DepthStencilCI {
        VkStencilOpState front = {};
        VkStencilOpState back = {};
        VkCompareOp depthCompareOp = VK_COMPARE_OP_ALWAYS;
        bool depthTestEnable = false;
        bool depthWriteEnable = false;
        bool stencilTestEnable = false;
    };

    struct ColorBlendAttachment {
        VkBlendFactor srcColor = VK_BLEND_FACTOR_ONE;
        VkBlendFactor dstColor = VK_BLEND_FACTOR_ONE;
        VkBlendOp colorOp = VK_BLEND_OP_ADD;

        VkBlendFactor srcAlpha = VK_BLEND_FACTOR_ONE;
        VkBlendFactor dstAlpha = VK_BLEND_FACTOR_ONE;
        VkBlendOp alphaOp = VK_BLEND_OP_ADD;

        bool enable = false;
        VkColorComponentFlags colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                               VK_COLOR_COMPONENT_G_BIT |
                                               VK_COLOR_COMPONENT_B_BIT |
                                               VK_COLOR_COMPONENT_A_BIT;
    };

    struct ColorBlendCI {
        std::vector<ColorBlendAttachment> attachments;

        ColorBlendCI &addAttachment(ColorBlendAttachment attachment);
    };

    struct ReflectOutput {
        std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> descriptorSets;

        void addBinding(uint32_t set, uint32_t binding, uint32_t count,
                        VkDescriptorType type, VkShaderStageFlags stageFlag);

        size_t getSetCount() { return descriptorSets.size(); }
    };

    struct RenderingCI {
        std::vector<VkFormat> colorFormats;
        VkFormat depthFormat = VK_FORMAT_UNDEFINED;
        VkFormat stencilFormat = VK_FORMAT_UNDEFINED;
    };

    struct PipelineCI {
        std::vector<ShaderStage> shaderStages;
        VertexInputCI vertexInput;
        RasterizationCI rasterization;
        DepthStencilCI depthStencil;
        ColorBlendCI colorBlend;
        RenderingCI rendering;
    };

    struct Pipeline {
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
        VkPipelineBindPoint bindPoint;
    };

    struct BufferCI {
        size_t size = 0;
        VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
        bool mapped = false;
        bool readback = false;
        std::string name;
    };

    struct Buffer {
        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo;
        size_t size;

        Handle<Buffer> handle;
        std::string name;
    };

    struct SamplerCI {
        VkFilter minFilter = VK_FILTER_NEAREST;
        VkFilter magFilter = VK_FILTER_NEAREST;
        VkSamplerMipmapMode mipFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST;

        VkSamplerAddressMode u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerAddressMode v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerAddressMode w = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        VkBorderColor borderColor = VK_BORDER_COLOR_MAX_ENUM;
    };

    struct Sampler {
        VkSampler sampler;
    };

    struct TextureCI {
        void* initialData = nullptr;
        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t depth = 1;
        uint32_t mipCount = 1;
        uint32_t layerCount = 1;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkImageType type = VK_IMAGE_TYPE_MAX_ENUM;
        VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
        std::string name;
    };

    struct Texture {
        VkImage image = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VmaAllocation allocation = nullptr;

        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t depth = 1;

        Handle<Texture> handle;
        std::string name;
    };

    struct Light {
        glm::vec3 position;
        float radius;

        glm::vec3 color;
        float intensity;
    };

    struct IndirectDrawData {
        VkDrawIndexedIndirectCommand cmd;

        uint32_t meshId;
        uint32_t materialOffset;
        uint32_t transformOffset;
    };

    struct Bounds {
        glm::vec3 origin;
        float radius;
        glm::vec3 extents;
        float pad;
    };
}