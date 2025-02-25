#pragma once

#include "../GpuResources.h"
#include "../RingBuffer.h"
#include "../AsyncLoader.h"

namespace Flare {
    struct GpuDevice;
    struct AsyncLoader;

    struct FrustumPlanes {
        glm::vec4 left;
        glm::vec4 right;
        glm::vec4 bottom;
        glm::vec4 top;
        glm::vec4 near;
        glm::vec4 far;
    };

    struct FrustumCullUniforms {
        FrustumPlanes frustumPlanes;

        uint32_t transformBufferIndex;
        uint32_t inputIndirectDrawDataBufferIndex;
        uint32_t outputIndirectDrawDataBufferIndex;
        uint32_t boundsBufferIndex;

        uint32_t countBufferIndex;
        uint32_t drawCount;
    };

    struct FrustumCullPass {
        void init(GpuDevice *gpuDevice);

        static FrustumPlanes getFrustumPlanes(glm::mat4 mat, bool normalize = true);

        void updateUniforms(AsyncLoader *asyncLoader);

        void cull(VkCommandBuffer cmd);

        void addBarriers(VkCommandBuffer cmd, uint32_t computeFamily, uint32_t mainFamily,
                         Handle<Buffer> outputIndirectDrawBufferHandle, Handle<Buffer> outputCountBufferHandle);

        void shutdown();

        GpuDevice *gpu = nullptr;

        PipelineCI pipelineCI;
        Handle<Pipeline> pipelineHandle;

        FrustumCullUniforms uniforms;
        RingBuffer frustumUniformRingBuffer;
    };
}
