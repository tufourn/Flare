#include "ShadowPass.h"

#include "../GpuDevice.h"
#include "../VkHelper.h"
#include "../AsyncLoader.h"

namespace Flare {
    void ShadowPass::init(GpuDevice *gpuDevice) {
        gpu = gpuDevice;

        TextureCI textureCI = {
                .width = gpu->swapchainExtent.width * SHADOW_RESOLUTION_MULTIPLIER,
                .height = gpu->swapchainExtent.height * SHADOW_RESOLUTION_MULTIPLIER,
                .depth = 1,
                .mipCount = 1,
                .layerCount = 1,
                .format = VK_FORMAT_D32_SFLOAT,
                .type = VK_IMAGE_TYPE_2D,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .name = "shadowDepthTexture",
        };
        depthTextureHandle = gpu->createTexture(textureCI);

        SamplerCI samplerCI = {
                .minFilter = VK_FILTER_NEAREST,
                .magFilter = VK_FILTER_NEAREST,
                .mipFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST,

                .u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                .v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                .w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,

                .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
        };
        samplerHandle = gpu->createSampler(samplerCI);

        pipelineCI = {
                .shaderStages = {
                        {"CoreShaders/ShadowPass.vert", VK_SHADER_STAGE_VERTEX_BIT},
                        {"CoreShaders/ShadowPass.frag", VK_SHADER_STAGE_FRAGMENT_BIT},
                },
                .rasterization = {
                        .cullMode = VK_CULL_MODE_FRONT_BIT,
                        .depthBiasEnable = true,
                        .depthBiasConstant = 0.01f,
                        .depthBiasSlope = 1.f,
                },
                .depthStencil = {
                        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
                        .depthTestEnable = true,
                        .depthWriteEnable = true,
                },
                .rendering = {
                        .depthFormat = VK_FORMAT_D32_SFLOAT,
                }
        };
        pipelineHandle = gpu->createPipeline(pipelineCI);

        BufferCI shadowUniformBufferCI = {
                .size = sizeof(ShadowUniform),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .name = "shadowUniforms",
        };
        shadowUniformRingBuffer.init(gpu, FRAMES_IN_FLIGHT, shadowUniformBufferCI, RingBuffer::Type::eUniform);
    }

    void ShadowPass::shutdown() {
        if (depthTextureHandle.isValid()) {
            gpu->destroyTexture(depthTextureHandle);
        }
        if (samplerHandle.isValid()) {
            gpu->destroySampler(samplerHandle);
        }
        if (pipelineHandle.isValid()) {
            gpu->destroyPipeline(pipelineHandle);
        }
        shadowUniformRingBuffer.shutdown();
    }

    void ShadowPass::render(VkCommandBuffer cmd,
                            Handle<Buffer> indexBuffer,
                            Handle<Buffer> indirectDrawBuffer, uint32_t count) {
        Texture *texture = gpu->getTexture(depthTextureHandle);
        Pipeline *pipeline = gpu->getPipeline(pipelineHandle);

        PushConstants pc{
                .uniformOffset = shadowUniformRingBuffer.buffer().index,
        };

        VkHelper::transitionImage(cmd, texture->image,
                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo depthAttachment = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = texture->imageView,
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = {
                        .depthStencil = {
                                .depth = 1.f,
                        }
                }
        };

        VkRenderingInfo renderingInfo = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .flags = 0,
                .renderArea = {VkOffset2D{0, 0}, {texture->width, texture->height}},
                .layerCount = 1,
                .viewMask = 0,
                .pDepthAttachment = &depthAttachment,
        };

        vkCmdBeginRendering(cmd, &renderingInfo);

        if (enable) {
            vkCmdBindPipeline(cmd, pipeline->bindPoint, pipeline->pipeline);
            vkCmdBindIndexBuffer(cmd, gpu->getBuffer(indexBuffer)->buffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdBindDescriptorSets(cmd, pipeline->bindPoint, pipeline->pipelineLayout, 0,
                                    gpu->bindlessDescriptorSets.size(), gpu->bindlessDescriptorSets.data(),
                                    0, nullptr);
            VkViewport viewport = {
                    .x = 0.f,
                    .y = 0.f,
                    .width = static_cast<float>(texture->width),
                    .height = static_cast<float>(texture->height),
                    .minDepth = 0.f,
                    .maxDepth = 1.f,
            };
            vkCmdSetViewport(cmd, 0, 1, &viewport);

            VkRect2D scissor = {
                    .offset = {0, 0},
                    .extent = {texture->width, texture->height},
            };
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants), &pc);

            vkCmdDrawIndexedIndirect(cmd, gpu->getBuffer(indirectDrawBuffer)->buffer, 0, count,
                                     sizeof(IndirectDrawData));
        }

        vkCmdEndRendering(cmd);

        VkHelper::transitionImage(cmd, texture->image,
                                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    }

    void ShadowPass::updateUniforms(AsyncLoader *asyncLoader) {
        shadowUniformRingBuffer.moveToNextBuffer();

        asyncLoader->uploadRequests.emplace_back(
                UploadRequest{
                        .dstBuffer = gpu->getUniform(shadowUniformRingBuffer.buffer()),
                        .data = (void *) &uniforms,
                }
        );
    }

}