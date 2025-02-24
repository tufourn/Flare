#pragma once

#include "../GpuResources.h"

namespace Flare {
    struct GpuDevice;

    struct FrustumCullPass {
        void init(GpuDevice *gpuDevice);

        void cull(Handle<Buffer> inputIndirectDrawBuffer,
                  Handle<Buffer> outputIndirectDraw,
                  Handle<Buffer> countBuffer);

        void addBarriers();

        void shutdown();

        GpuDevice *gpu = nullptr;

        PipelineCI pipelineCI;
        Handle<Pipeline> pipelineHandle;
    };
}
