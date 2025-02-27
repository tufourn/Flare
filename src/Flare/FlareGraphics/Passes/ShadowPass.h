#pragma once

#include "../GpuResources.h"
#include "../RingBuffer.h"

namespace Flare {
    struct GpuDevice;

    static constexpr uint32_t SHADOW_RESOLUTION = 2048;

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

        void updateUniforms();

        void render(VkCommandBuffer cmd,
                    Handle<Buffer> indexBuffer,
                    Handle<Buffer> indirectDrawBuffer,
                    uint32_t count);

        GpuDevice *gpu = nullptr;

        bool enable = true;

        Handle<Texture> depthTextureHandle;
        Handle<Sampler> samplerHandle;

        PipelineCI pipelineCI;
        Handle<Pipeline> pipelineHandle;

        ShadowUniform uniforms;
        RingBuffer shadowUniformRingBuffer;
    };
}
