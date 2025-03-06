#pragma once

#include "../GpuResources.h"
#include "../RingBuffer.h"

namespace Flare {
    struct GpuDevice;

    static constexpr uint32_t SHADOW_RESOLUTION = 2048;

    struct ShadowInputs {
        Handle<Buffer> positionBuffer;
        Handle<Buffer> transformBuffer;
        Handle<Buffer> lightBuffer;

        Handle<Buffer> indexBuffer;
        Handle<Buffer> indirectDrawBuffer;
        Handle<Buffer> countBuffer;
        uint32_t maxDrawCount;
    };

    struct ShadowPass {
        void init(GpuDevice *gpuDevice);

        void shutdown();

        void setInputs(const ShadowInputs& inputs);

        void render(VkCommandBuffer cmd);

        GpuDevice *gpu = nullptr;

        bool enable = true;

        Handle<Texture> depthTextureHandle;
        Handle<Sampler> samplerHandle;

        PipelineCI pipelineCI;
        Handle<Pipeline> pipelineHandle;

        PushConstants pc;
        Handle<Buffer> indexBuffer;
        Handle<Buffer> indirectDrawBuffer;
        Handle<Buffer> countBuffer;
        uint32_t maxDrawCount = UINT32_MAX;
    };
}
