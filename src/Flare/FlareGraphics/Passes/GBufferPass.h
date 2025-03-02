#pragma once

#include "../GpuResources.h"

namespace Flare {
    struct GpuDevice;

    struct GBufferPass {
        void init(GpuDevice *gpuDevice);

        void render(VkCommandBuffer cmd);

        void shutdown();

        void genRenderTargets(uint32_t width, uint32_t height);

        GpuDevice *gpu = nullptr;

        bool loaded = false;

        Handle<Texture> normalTargetHandle;

        PipelineCI pipelineCI;
        Handle<Pipeline> pipelineHandle;
    };
}
