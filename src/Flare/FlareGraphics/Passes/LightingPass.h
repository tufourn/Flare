#pragma once

#include "../GpuResources.h"

namespace Flare {
    struct GpuDevice;

    struct LightingPassInputs {
        Handle<Buffer> cameraBuffer;
        Handle<Buffer> lightBuffer;

        Handle<Texture> gBufferAlbedo;
        Handle<Texture> gBufferNormal;
        Handle<Texture> gBufferOcclusionMetallicRoughness;
        Handle<Texture> gBufferEmissive;
        Handle<Texture> gBufferDepth;

        Handle<Texture> shadowMap;
    };

    struct LightingPass {
        void init(GpuDevice* gpuDevice);

        void shutdown();

        void render(VkCommandBuffer cmd);

        void generateRenderTarget();

        void destroyRenderTarget();

        GpuDevice* gpu = nullptr;

        PipelineCI pipelineCI;
        Handle<Pipeline> pipelineHandle;
        Handle<Texture> targetHandle;

        LightingPassInputs inputs;

        bool loaded = false;
    };
}
