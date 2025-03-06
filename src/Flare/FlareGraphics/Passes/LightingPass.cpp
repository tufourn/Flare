#include "LightingPass.h"
#include "../GpuDevice.h"
#include "../VkHelper.h"

namespace Flare {
    void LightingPass::init(GpuDevice *gpuDevice) {
        gpu = gpuDevice;

        pipelineCI.shaderStages = {
                {"CoreShaders/FullscreenTriangle.vert", VK_SHADER_STAGE_VERTEX_BIT},
                {"CoreShaders/LightingPass.frag",       VK_SHADER_STAGE_FRAGMENT_BIT},
        };
        pipelineCI.rendering.colorFormats = {
                VK_FORMAT_R32G32B32A32_SFLOAT,
        };
        pipelineHandle = gpu->createPipeline(pipelineCI);

        generateRenderTarget();

        loaded = true;
    }

    void LightingPass::shutdown() {
        destroyRenderTarget();
        gpu->destroyPipeline(pipelineHandle);
    }

    void LightingPass::generateRenderTarget() {
        if (loaded) {
            destroyRenderTarget();
        }

        TextureCI targetCI = {
                .width = gpu->swapchainExtent.width,
                .height = gpu->swapchainExtent.height,
                .depth = 1,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .type = VK_IMAGE_TYPE_2D,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .name = "lighting pass",
                .offscreenDraw = true,
        };
        targetHandle = gpu->createTexture(targetCI);

    }

    void LightingPass::destroyRenderTarget() {
        gpu->destroyTexture(targetHandle);
    }

    void LightingPass::render(VkCommandBuffer cmd) {
        Pipeline *pipeline = gpu->getPipeline(pipelineHandle);

        VkRenderingAttachmentInfo colorAttachment = VkHelper::colorAttachment(gpu->getTexture(targetHandle)->imageView);
        VkRenderingInfo renderingInfo = VkHelper::renderingInfo(gpu->swapchainExtent, 1, &colorAttachment);

        PushConstants pc {};
        pc.data0 = inputs.cameraBuffer.index;
        pc.data1 = inputs.lightBuffer.index;
        pc.data2 = inputs.gBufferAlbedo.index;
        pc.data3 = inputs.gBufferNormal.index;
        pc.data4 = inputs.gBufferOcclusionMetallicRoughness.index;
        pc.data5 = inputs.gBufferEmissive.index;
        pc.data6 = inputs.gBufferDepth.index;

        VkHelper::transitionImage(cmd, gpu->getTexture(targetHandle)->image,
                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        vkCmdBeginRendering(cmd, &renderingInfo);
        vkCmdBindPipeline(cmd, pipeline->bindPoint, pipeline->pipeline);

        vkCmdBindDescriptorSets(cmd, pipeline->bindPoint, pipeline->pipelineLayout, 0,
                                gpu->bindlessDescriptorSets.size(), gpu->bindlessDescriptorSets.data(),
                                0, nullptr);

        vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants), &pc);

        VkViewport viewport = VkHelper::viewport(gpu->swapchainExtent);
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor = VkHelper::scissor(gpu->swapchainExtent);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdDraw(cmd, 3, 1, 0, 0); // fullscreen triangle

        vkCmdEndRendering(cmd);
    }
}