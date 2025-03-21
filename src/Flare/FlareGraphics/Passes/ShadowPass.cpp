#include "ShadowPass.h"

#include "../GpuDevice.h"
#include "../VkHelper.h"

namespace Flare {
void ShadowPass::init(GpuDevice *gpuDevice) {
  gpu = gpuDevice;

  TextureCI textureCI = {
      .width = SHADOW_RESOLUTION,
      .height = SHADOW_RESOLUTION,
      .depth = 1,
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
      .shaderStages =
          {
              {"CoreShaders/ShadowPass.vert", VK_SHADER_STAGE_VERTEX_BIT},
              {"CoreShaders/ShadowPass.frag", VK_SHADER_STAGE_FRAGMENT_BIT},
          },
      .rasterization =
          {
              .cullMode = VK_CULL_MODE_FRONT_BIT,
              .depthBiasEnable = true,
              .depthBiasConstant = 0.01f,
              .depthBiasSlope = 1.f,
          },
      .depthStencil =
          {
              .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
              .depthTestEnable = true,
              .depthWriteEnable = true,
          },
      .rendering = {
          .depthFormat = VK_FORMAT_D32_SFLOAT,
      }};
  pipelineHandle = gpu->createPipeline(pipelineCI);
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
}

void ShadowPass::render(VkCommandBuffer cmd) {
  Texture *texture = gpu->getTexture(depthTextureHandle);
  Pipeline *pipeline = gpu->getPipeline(pipelineHandle);

  VkHelper::transitionImage(cmd, texture->image, VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  VkRenderingAttachmentInfo depthAttachment =
      VkHelper::depthAttachment(texture->imageView);

  VkRenderingInfo renderingInfo = VkHelper::renderingInfo(
      texture->width, texture->height, 0, nullptr, &depthAttachment);

  vkCmdBeginRendering(cmd, &renderingInfo);

  if (enable && maxDrawCount > 0) {
    vkCmdBindPipeline(cmd, pipeline->bindPoint, pipeline->pipeline);
    vkCmdBindIndexBuffer(cmd, gpu->getBuffer(indexBufferHandle)->buffer, 0,
                         VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(cmd, pipeline->bindPoint, pipeline->pipelineLayout,
                            0, gpu->bindlessDescriptorSets.size(),
                            gpu->bindlessDescriptorSets.data(), 0, nullptr);

    VkViewport viewport = VkHelper::viewport(texture->width, texture->height);
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = VkHelper::scissor(texture->width, texture->height);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_ALL, 0,
                       sizeof(PushConstants), &pc);

    vkCmdDrawIndexedIndirectCount(
        cmd, gpu->getBuffer(indirectDrawBufferHandle)->buffer, 0,
        gpu->getBuffer(countBufferHandle)->buffer, 0, maxDrawCount,
        sizeof(IndirectDrawData));
  }

  vkCmdEndRendering(cmd);

  VkHelper::transitionImage(cmd, texture->image,
                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
}

void ShadowPass::setInputs(const ShadowInputs &inputs) {
  pc.data0 = inputs.indirectDrawBuffer.index;
  pc.data1 = inputs.positionBuffer.index;
  pc.data2 = inputs.transformBuffer.index;
  pc.data3 = inputs.lightBuffer.index;

  indexBufferHandle = inputs.indexBuffer;
  indirectDrawBufferHandle = inputs.indirectDrawBuffer;
  countBufferHandle = inputs.countBuffer;
  maxDrawCount = inputs.maxDrawCount;
}
} // namespace Flare