#include "GBufferPass.h"

#include "../GpuDevice.h"

namespace Flare {
    void GBufferPass::init(GpuDevice *gpuDevice) {
        gpu = gpuDevice;

        pipelineCI.shaderStages = {
                {"CoreShaders/"}
        };

        genRenderTargets(800, 600);
    }

    void GBufferPass::shutdown() {
        gpu->destroyTexture(normalTargetHandle);
    }

    void GBufferPass::genRenderTargets(uint32_t width, uint32_t height) {
        if (loaded) {
            gpu->destroyTexture(normalTargetHandle);
        }

        TextureCI normalTargetCI = {
                .width = width,
                .height = height,
                .depth = 1,
                .format = VK_FORMAT_R16G16_SFLOAT,
                .type = VK_IMAGE_TYPE_2D,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .name = "gbuffer normal",
                .offscreenDraw = true,
        };
        normalTargetHandle = gpu->createTexture(normalTargetCI);
    }

    void GBufferPass::render(VkCommandBuffer cmd) {

    }
}
