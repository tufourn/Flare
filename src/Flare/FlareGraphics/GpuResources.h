#pragma once

#include <volk.h>
#include <cstdint>
#include <cassert>
#include <vector>
#include <spdlog/spdlog.h>

namespace Flare {
    constexpr uint32_t invalidIndex = 0xFFFFFFFF;

    template<typename T>
    struct Handle {
        uint32_t index;

        bool isValid() const { return index != invalidIndex; }
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

        const T& get(Handle<T> handle) const {
            if (handle.isValid() && handle.index < poolSize) {
                return data[handle.index];
            } else {
                spdlog::error("ResourcePool: Invalid handle access");
            }
        }
    };

    struct Shader {
        const char *code = nullptr;
        uint32_t size = 0;
        VkShaderStageFlagBits type = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
    };

    enum class ExecModel : uint32_t {
        eNone = 0,
        eVertex = 1,
        eFragment = 2,
        eCompute = 4,
    };

    inline ExecModel operator|(ExecModel lhs, ExecModel rhs) {
        return static_cast<ExecModel>(
                static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs)
        );
    }

    inline ExecModel operator&(ExecModel lhs, ExecModel rhs) {
        return static_cast<ExecModel>(
                static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)
        );
    }

    struct ShaderStageCI {
        std::vector<Shader> shaders;

        ShaderStageCI &addStage(Shader stage);
    };

    struct VertexInputCI {
        std::vector<VkVertexInputBindingDescription> vertexBindings;
        std::vector<VkVertexInputAttributeDescription> vertexAttributes;

        VertexInputCI &addBinding(VkVertexInputBindingDescription binding);

        VertexInputCI &addAttribute(VkVertexInputAttributeDescription attribute);
    };

    struct RasterizationCI {
        VkCullModeFlagBits cullMode = VK_CULL_MODE_NONE;
        VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    };

    struct DepthStencilCI {
        VkStencilOpState front = {
                .failOp = VK_STENCIL_OP_KEEP,
                .passOp = VK_STENCIL_OP_KEEP,
                .depthFailOp = VK_STENCIL_OP_KEEP,
                .compareOp = VK_COMPARE_OP_ALWAYS,
                .compareMask = 0xFF,
                .writeMask = 0xFF,
                .reference = 0xFF,
        };
        VkStencilOpState back = {
                .failOp = VK_STENCIL_OP_KEEP,
                .passOp = VK_STENCIL_OP_KEEP,
                .depthFailOp = VK_STENCIL_OP_KEEP,
                .compareOp = VK_COMPARE_OP_ALWAYS,
                .compareMask = 0xFF,
                .writeMask = 0xFF,
                .reference = 0xFF,
        };
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

    struct DescriptorSetLayoutCI {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        uint32_t index = 0;
    };

    struct PipelineCI {
        ShaderStageCI shaderStages;
        VertexInputCI vertexInput;
        RasterizationCI rasterization;
        DepthStencilCI depthStencil;
        ColorBlendCI colorBlend;
        bool isCompute = false;
    };

    struct Pipeline {
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
        VkPipelineBindPoint bindPoint;
    };
}