#include "DrawBoundsPass.h"
#include "../GpuDevice.h"
#include "../VkHelper.h"

namespace Flare {
void DrawBoundsPass::init(GpuDevice *gpuDevice) {
  gpu = gpuDevice;
  pipelineCI.shaderStages = {
      {"CoreShaders/DrawBounds.vert", VK_SHADER_STAGE_VERTEX_BIT},
      {"CoreShaders/DrawBounds.frag", VK_SHADER_STAGE_FRAGMENT_BIT},
  };
  pipelineCI.rendering.colorFormats = {gpu->drawTextureFormat};
  pipelineCI.vertexInput
      .addBinding({
          .binding = 0,
          .stride = sizeof(glm::vec4),
          .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
      })
      .addAttribute({.location = 0,
                     .binding = 0,
                     .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                     .offset = 0});
  pipelineCI.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
  pipelineHandle = gpu->createPipeline(pipelineCI);

  BufferCI vertexCI = {
      .initialData = (void *)boxCorners.data(),
      .size = sizeof(glm::vec4) * boxCorners.size(),
      .usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      .mapped = true,
      .name = "frustum vertex buffer",
  };
  vertexBufferHandle = gpu->createBuffer(vertexCI);

  BufferCI indexCI = {
      .initialData = (void *)boxIndices.data(),
      .size = sizeof(uint32_t) * boxIndices.size(),
      .usageFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      .mapped = true,
      .name = "frustum index buffer",
  };
  indexBufferHandle = gpu->createBuffer(indexCI);
}

void DrawBoundsPass::shutdown() {
  gpu->destroyPipeline(pipelineHandle);
  gpu->destroyBuffer(vertexBufferHandle);
  gpu->destroyBuffer(indexBufferHandle);
}

void DrawBoundsPass::setInputs(const DrawBoundsInputs &inputs) {
  drawBoundsInputs = inputs;
}

void DrawBoundsPass::render(VkCommandBuffer cmd) {
  if (drawBoundsInputs.count == 0) {
    return;
  }
  Pipeline *pipeline = gpu->getPipeline(pipelineHandle);

  VkRenderingAttachmentInfo colorAttachment = VkHelper::colorAttachment(
      gpu->getTexture(gpu->drawTexture)->imageView, VK_ATTACHMENT_LOAD_OP_LOAD,
      VK_ATTACHMENT_STORE_OP_STORE);

  VkRenderingInfo renderingInfo =
      VkHelper::renderingInfo(gpu->swapchainExtent, 1, &colorAttachment);

  vkCmdBeginRendering(cmd, &renderingInfo);
  vkCmdBindPipeline(cmd, pipeline->bindPoint, pipeline->pipeline);
  vkCmdBindIndexBuffer(cmd, gpu->getBuffer(indexBufferHandle)->buffer, 0,
                       VK_INDEX_TYPE_UINT32);
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(cmd, 0, 1, &gpu->getBuffer(vertexBufferHandle)->buffer,
                         offsets);

  VkViewport viewport = VkHelper::viewport(gpu->swapchainExtent);
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = VkHelper::scissor(gpu->swapchainExtent);
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  for (size_t i = 0; i < drawBoundsInputs.count; i++) {
    PushConstants pc;
    pc.mat = drawBoundsInputs.viewProjection;
    pc.data0 = drawBoundsInputs.boundsBuffer.index;
    pc.data1 = drawBoundsInputs.transformBuffer.index;
    pc.data2 = i;
    vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_ALL, 0,
                       sizeof(PushConstants), &pc);
    vkCmdDrawIndexed(cmd, boxIndices.size(), 1, 0, 0, 0);
  }

  vkCmdEndRendering(cmd);
}
} // namespace Flare