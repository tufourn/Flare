#pragma once

#include "../GpuResources.h"

namespace Flare {
    struct GpuDevice;

    static constexpr uint32_t SHADOW_RESOLUTION_MULTIPLIER = 4; // render shadows at 4x resolution of swapchain image

    struct ShadowUniform {
        glm::mat4 lightSpaceMatrix;

        uint32_t indirectDrawDataBufferIndex;
        uint32_t positionBufferIndex;
        uint32_t transformBufferIndex;
        float pad;
    };

    struct ShadowPass {
        void init(GpuDevice *gpuDevice);

        void shutdown();

        void render(VkCommandBuffer cmd,
                    uint32_t bufferOffset,
                    Handle<Buffer> indexBuffer,
                    Handle<Buffer> indirectDrawBuffer,
                    uint32_t count);

        GpuDevice *gpu = nullptr;

        Handle<Texture> depthTextureHandle;
        Handle<Sampler> samplerHandle;

        PipelineCI pipelineCI;
        Handle<Pipeline> pipelineHandle;
    };
}
