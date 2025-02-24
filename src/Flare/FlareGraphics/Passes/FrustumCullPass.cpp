#include "FrustumCullPass.h"

#include "../GpuDevice.h"

namespace Flare {
    void FrustumCullPass::init(GpuDevice *gpuDevice) {
        gpu = gpuDevice;

        pipelineCI.shaderStages = {
                {"CoreShaders/FrustumCull.comp", VK_SHADER_STAGE_COMPUTE_BIT},
        };
        pipelineHandle = gpu->createPipeline(pipelineCI);
    }

    void FrustumCullPass::addBarriers() {

    }

    void FrustumCullPass::shutdown() {
        if (pipelineHandle.isValid()) {
            gpu->destroyPipeline(pipelineHandle);
        }
    }

    void FrustumCullPass::cull(Handle<Buffer> inputIndirectDrawBuffer,
                               Handle<Buffer> outputIndirectDraw,
                               Handle<Buffer> countBuffer) {

    }
}