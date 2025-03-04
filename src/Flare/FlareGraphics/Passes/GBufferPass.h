#pragma once

#include "../GpuResources.h"
#include "../RingBuffer.h"

namespace Flare {
    struct GpuDevice;

    struct GBufferUniforms {
        glm::mat4 viewProjection = glm::mat4(1.f);
        glm::mat4 prevViewProjection = glm::mat4(1.f);
    };

    struct GBufferPass {
        void init(GpuDevice *gpuDevice);

        void setBuffers(const MeshDrawBuffers& buffers);

        void render(VkCommandBuffer cmd);

        void destroyRenderTargets();

        void shutdown();

        void generateRenderTargets();

        void updateViewProjection(glm::mat4 viewProjection);

        GpuDevice *gpu = nullptr;

        bool loaded = false;

        Handle<Texture> depthTargetHandle;
        Handle<Texture> albedoTargetHandle;
        Handle<Texture> normalTargetHandle;
        Handle<Texture> occlusionMetallicRoughnessTargetHandle;
        Handle<Texture> emissiveTargetHandle;

        PipelineCI pipelineCI;
        Handle<Pipeline> pipelineHandle;

        MeshDrawBuffers meshDrawBuffers;

        GBufferUniforms uniforms;
        RingBuffer gBufferUniformRingBuffer;
    };
}
