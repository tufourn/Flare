#include "GBufferPass.h"

#include "../GpuDevice.h"
#include "../VkHelper.h"

namespace Flare {
    void GBufferPass::init(GpuDevice *gpuDevice) {
        gpu = gpuDevice;

        BufferCI uniformCI = {
                .size = sizeof(GBufferUniforms),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                .name = "G buffer uniforms",
                .bufferType = BufferType::eUniform,

        };
        gBufferUniformRingBuffer.init(gpu, FRAMES_IN_FLIGHT, uniformCI);

        pipelineCI.shaderStages = {
                {"CoreShaders/GBuffer.vert", VK_SHADER_STAGE_VERTEX_BIT},
                {"CoreShaders/GBuffer.frag", VK_SHADER_STAGE_FRAGMENT_BIT},
        };
        pipelineCI.rendering.colorFormats = {
                VK_FORMAT_R8G8B8A8_UNORM, // albedo
                VK_FORMAT_R16G16_SFLOAT, // normal
                VK_FORMAT_R8G8B8A8_UNORM, // occlusion metallic roughness
                VK_FORMAT_R8G8B8A8_UNORM, // emissive
        };
        pipelineCI.rendering.depthFormat = VK_FORMAT_D32_SFLOAT;
        pipelineCI.depthStencil = {
                .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
                .depthTestEnable = true,
                .depthWriteEnable = true,
        };
        pipelineCI.vertexInput
                        // position
                .addBinding({.binding = 0, .stride = sizeof(glm::vec4), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX})
                .addAttribute({.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = 0})
                        // uv
                .addBinding({.binding = 1, .stride = sizeof(glm::vec2), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX})
                .addAttribute({.location = 1, .binding = 1, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 0})
                        // normal
                .addBinding({.binding = 2, .stride = sizeof(glm::vec4), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX})
                .addAttribute({.location = 2, .binding = 2, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = 0})
                        // tangent
                .addBinding({.binding = 3, .stride = sizeof(glm::vec4), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX})
                .addAttribute({.location = 3, .binding = 3, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = 0});
        pipelineHandle = gpu->createPipeline(pipelineCI);

        generateRenderTargets();
    }

    void GBufferPass::shutdown() {
        destroyRenderTargets();
        gpu->destroyPipeline(pipelineHandle);
        gBufferUniformRingBuffer.shutdown();
    }

    void GBufferPass::generateRenderTargets() {
        if (loaded) {
            destroyRenderTargets();
        }

        TextureCI depthTargetCI = {
                .width = gpu->swapchainExtent.width,
                .height = gpu->swapchainExtent.height,
                .depth = 1,
                .format = VK_FORMAT_D32_SFLOAT,
                .type = VK_IMAGE_TYPE_2D,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .name = "gbuffer depth",
        };
        depthTargetHandle = gpu->createTexture(depthTargetCI);

        TextureCI albedoTargetCI = {
                .width = gpu->swapchainExtent.width,
                .height = gpu->swapchainExtent.height,
                .depth = 1,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .type = VK_IMAGE_TYPE_2D,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .name = "gbuffer albedo",
                .offscreenDraw = true,
        };
        albedoTargetHandle = gpu->createTexture(albedoTargetCI);

        TextureCI normalTargetCI = {
                .width = gpu->swapchainExtent.width,
                .height = gpu->swapchainExtent.height,
                .depth = 1,
                .format = VK_FORMAT_R16G16_SFLOAT, // octahedral pack
                .type = VK_IMAGE_TYPE_2D,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .name = "gbuffer normal",
                .offscreenDraw = true,
        };
        normalTargetHandle = gpu->createTexture(normalTargetCI);

        TextureCI occlusionMetallicRoughnessCI = {
                .width = gpu->swapchainExtent.width,
                .height = gpu->swapchainExtent.height,
                .depth = 1,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .type = VK_IMAGE_TYPE_2D,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .name = "gbuffer occlusion metallic roughness",
                .offscreenDraw = true,
        };
        occlusionMetallicRoughnessTargetHandle = gpu->createTexture(occlusionMetallicRoughnessCI);

        TextureCI emissiveCI = {
                .width = gpu->swapchainExtent.width,
                .height = gpu->swapchainExtent.height,
                .depth = 1,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .type = VK_IMAGE_TYPE_2D,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .name = "gbuffer emissive",
                .offscreenDraw = true,
        };
        emissiveTargetHandle = gpu->createTexture(emissiveCI);

        loaded = true;
    }

    void GBufferPass::setBuffers(const MeshDrawBuffers &buffers) {
        meshDrawBuffers = buffers;
    }

    void GBufferPass::render(VkCommandBuffer cmd) {
        Pipeline *pipeline = gpu->getPipeline(pipelineHandle);

        std::array<VkRenderingAttachmentInfo, 4> colorAttachments = {
                VkHelper::colorAttachment(gpu->getTexture(albedoTargetHandle)->imageView),
                VkHelper::colorAttachment(gpu->getTexture(normalTargetHandle)->imageView),
                VkHelper::colorAttachment(gpu->getTexture(occlusionMetallicRoughnessTargetHandle)->imageView),
                VkHelper::colorAttachment(gpu->getTexture(emissiveTargetHandle)->imageView),
        };

        VkRenderingAttachmentInfo depthAttachment = VkHelper::depthAttachment(
                gpu->getTexture(depthTargetHandle)->imageView
        );

        VkRenderingInfo renderingInfo = VkHelper::renderingInfo(
                gpu->swapchainExtent,
                colorAttachments.size(), colorAttachments.data(),
                &depthAttachment
        );

        std::array<VkBuffer, 4> vertexBuffers = {
                gpu->getBuffer(meshDrawBuffers.positions)->buffer,
                gpu->getBuffer(meshDrawBuffers.uvs)->buffer,
                gpu->getBuffer(meshDrawBuffers.normals)->buffer,
                gpu->getBuffer(meshDrawBuffers.tangents)->buffer,
        };
        VkDeviceSize offsets[] = {0, 0, 0, 0};

        PushConstants pc{};
        pc.uniformOffset = gBufferUniformRingBuffer.buffer().index;
        pc.data0 = meshDrawBuffers.indirectDraws.index;
        pc.data1 = meshDrawBuffers.transforms.index;
        pc.data2 = meshDrawBuffers.materials.index;
        pc.data3 = meshDrawBuffers.textures.index;

        VkHelper::transitionImage(cmd, gpu->getTexture(albedoTargetHandle)->image,
                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        vkCmdBeginRendering(cmd, &renderingInfo);
        vkCmdBindPipeline(cmd, pipeline->bindPoint, pipeline->pipeline);
        vkCmdBindIndexBuffer(cmd, gpu->getBuffer(meshDrawBuffers.indices)->buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindVertexBuffers(cmd, 0, vertexBuffers.size(), vertexBuffers.data(), offsets);
        vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_ALL, 0,
                           sizeof(PushConstants), &pc);
        vkCmdBindDescriptorSets(cmd, pipeline->bindPoint, pipeline->pipelineLayout, 0,
                                gpu->bindlessDescriptorSets.size(), gpu->bindlessDescriptorSets.data(),
                                0, nullptr);

        VkViewport viewport = VkHelper::viewport(gpu->swapchainExtent);
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor = VkHelper::scissor(gpu->swapchainExtent);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdDrawIndexedIndirectCount(cmd,
                                      gpu->getBuffer(meshDrawBuffers.indirectDraws)->buffer, 0,
                                      gpu->getBuffer(meshDrawBuffers.count)->buffer, 0,
                                      meshDrawBuffers.drawCount,
                                      sizeof(IndirectDrawData));

        vkCmdEndRendering(cmd);
        VkHelper::transitionImage(cmd, gpu->getTexture(albedoTargetHandle)->image,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void GBufferPass::destroyRenderTargets() {
        gpu->destroyTexture(depthTargetHandle);
        gpu->destroyTexture(albedoTargetHandle);
        gpu->destroyTexture(normalTargetHandle);
        gpu->destroyTexture(occlusionMetallicRoughnessTargetHandle);
        gpu->destroyTexture(emissiveTargetHandle);
    }

    void GBufferPass::updateViewProjection(glm::mat4 viewProjection) {
        uniforms.prevViewProjection = uniforms.viewProjection;
        uniforms.viewProjection = viewProjection;

        gBufferUniformRingBuffer.moveToNextBuffer();
        gpu->uploadBufferData(gBufferUniformRingBuffer.buffer(), &uniforms);
    }
}
