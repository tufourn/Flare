#pragma once

#include "../GpuResources.h"
#include "../RingBuffer.h"

namespace Flare {
    struct GpuDevice;

    static constexpr uint32_t SHADOW_RESOLUTION = 2048;

    struct ShadowUniform {
        glm::mat4 lightSpaceMatrix;
    };

    struct ShadowPass {
        void init(GpuDevice *gpuDevice);

        void shutdown();

        void updateUniforms();

        void setBuffers(Handle<Buffer> indirectBuffer, Handle<Buffer> positionBuffer, Handle<Buffer> transformBuffer);

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

        PushConstants pc;
    };
}
