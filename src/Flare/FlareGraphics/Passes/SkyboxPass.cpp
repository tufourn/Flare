#include "SkyboxPass.h"

#include "../BasicGeometry.h"
#include "../GpuDevice.h"
#include "../VkHelper.h"
#include "glm/ext/matrix_clip_space.hpp"
#include <numbers>
#include <stb_image.h>

namespace Flare {

void SkyboxPass::init(GpuDevice *gpuDevice) {
  gpu = gpuDevice;

  TextureCI cubemapCI = {
      .width = SKYBOX_RESOLUTION,
      .height = SKYBOX_RESOLUTION,
      .depth = 1,
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .type = VK_IMAGE_TYPE_2D,
      .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
      .name = "skybox",
      .genMips = true,
      .cubemap = true,
  };

  skyboxHandle = gpu->createTexture(cubemapCI);

  cubemapCI.name = "irradianceMap";
  irradianceMapHandle = gpu->createTexture(cubemapCI);

  cubemapCI.name = "prefilteredCube";
  prefilteredCubeHandle = gpu->createTexture(cubemapCI);

  TextureCI offscreenImageCI = {
      .width = SKYBOX_RESOLUTION,
      .height = SKYBOX_RESOLUTION,
      .depth = 1,
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .type = VK_IMAGE_TYPE_2D,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .name = "skyboxOffscreen",
      .offscreenDraw = true,
  };
  offscreenImageHandle = gpu->createTexture(offscreenImageCI);

  offscreenImageCI.name = "brdfLut";
  brdfLutHandle = gpu->createTexture(offscreenImageCI);

  SamplerCI samplerCI = {
      .minFilter = VK_FILTER_LINEAR,
      .magFilter = VK_FILTER_LINEAR,
      .mipFilter = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
  };
  samplerHandle = gpu->createSampler(samplerCI);

  MeshBuffers cubeMesh = createCubeMesh(2.0, 2.0, 2.0);
  BufferCI vertexBufferCI = {
      .initialData = cubeMesh.positions.data(),
      .size = cubeMesh.positions.size() * sizeof(glm::vec4),
      .usageFlags =
          VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      .name = "cubemapPositionBuffer",
  };
  vertexBufferHandle = gpu->createBuffer(vertexBufferCI);

  BufferCI indexBufferCI = {
      .initialData = cubeMesh.indices.data(),
      .size = cubeMesh.indices.size() * sizeof(uint32_t),
      .usageFlags =
          VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      .name = "cubemapIndexBuffer",
  };
  indexBufferHandle = gpu->createBuffer(indexBufferCI);

  PipelineCI offscreenPipelineCI;
  offscreenPipelineCI.rendering.colorFormats.push_back(
      VK_FORMAT_R32G32B32A32_SFLOAT);

  offscreenPipelineCI.shaderStages = {
      ShaderStage{"CoreShaders/FullscreenTriangle.vert",
                  VK_SHADER_STAGE_VERTEX_BIT},
      ShaderStage{"CoreShaders/BrdfLut.frag", VK_SHADER_STAGE_FRAGMENT_BIT},
  };
  brdfLutPipelineHandle = gpu->createPipeline(offscreenPipelineCI);

  offscreenPipelineCI.shaderStages = {
      ShaderStage{"CoreShaders/Cubemap.vert", VK_SHADER_STAGE_VERTEX_BIT},
      ShaderStage{"CoreShaders/Cubemap.frag", VK_SHADER_STAGE_FRAGMENT_BIT},
  };
  offscreenPipelineCI.vertexInput
      .addBinding({.binding = 0,
                   .stride = sizeof(glm::vec4),
                   .inputRate = VK_VERTEX_INPUT_RATE_VERTEX})
      .addAttribute({.location = 0,
                     .binding = 0,
                     .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                     .offset = 0});
  cubemapPipelineHandle = gpu->createPipeline(offscreenPipelineCI);

  offscreenPipelineCI.shaderStages = {
      ShaderStage{"CoreShaders/Cubemap.vert", VK_SHADER_STAGE_VERTEX_BIT},
      ShaderStage{"CoreShaders/IrradianceMap.frag",
                  VK_SHADER_STAGE_FRAGMENT_BIT},
  };
  irradianceMapPipelineHandle = gpu->createPipeline(offscreenPipelineCI);

  offscreenPipelineCI.shaderStages = {
      ShaderStage{"CoreShaders/Cubemap.vert", VK_SHADER_STAGE_VERTEX_BIT},
      ShaderStage{"CoreShaders/PrefilteredCube.frag",
                  VK_SHADER_STAGE_FRAGMENT_BIT},
  };
  prefilteredCubePipelineHandle = gpu->createPipeline(offscreenPipelineCI);

  PipelineCI skyboxPipelineCI;
  skyboxPipelineCI.shaderStages = {
      ShaderStage{"CoreShaders/Skybox.vert", VK_SHADER_STAGE_VERTEX_BIT},
      ShaderStage{"CoreShaders/Skybox.frag", VK_SHADER_STAGE_FRAGMENT_BIT},
  };
  skyboxPipelineCI.rendering.colorFormats.push_back(
      VK_FORMAT_R32G32B32A32_SFLOAT);
  skyboxPipelineCI.rendering.depthFormat = VK_FORMAT_D32_SFLOAT;
  skyboxPipelineCI.depthStencil = {
      .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
      .depthTestEnable = true,
      .depthWriteEnable = false,
  };
  skyboxPipelineCI.vertexInput
      .addBinding({.binding = 0,
                   .stride = sizeof(glm::vec4),
                   .inputRate = VK_VERTEX_INPUT_RATE_VERTEX})
      .addAttribute({.location = 0,
                     .binding = 0,
                     .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                     .offset = 0});
  skyboxPipelineHandle = gpu->createPipeline(skyboxPipelineCI);
}

void SkyboxPass::shutdown() {
  gpu->destroyTexture(loadedImageHandle);
  gpu->destroyTexture(skyboxHandle);
  gpu->destroyTexture(irradianceMapHandle);
  gpu->destroyTexture(prefilteredCubeHandle);
  gpu->destroyTexture(brdfLutHandle);
  gpu->destroyTexture(offscreenImageHandle);

  gpu->destroySampler(samplerHandle);

  gpu->destroyBuffer(vertexBufferHandle);
  gpu->destroyBuffer(indexBufferHandle);

  gpu->destroyPipeline(cubemapPipelineHandle);
  gpu->destroyPipeline(skyboxPipelineHandle);
  gpu->destroyPipeline(irradianceMapPipelineHandle);
  gpu->destroyPipeline(prefilteredCubePipelineHandle);
  gpu->destroyPipeline(brdfLutPipelineHandle);
}

void SkyboxPass::loadImage(std::filesystem::path path) {
  if (loaded) {
    return; // todo: reload image
  }

  int width, height, components;
  unsigned char *stbData = stbi_load(path.string().c_str(), &width, &height,
                                     &components, STBI_rgb_alpha);

  if (stbData) {
    TextureCI textureCI = {
        .initialData = stbData,
        .width = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
        .depth = 1,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .type = VK_IMAGE_TYPE_2D,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .name = path.filename().string().c_str(),
    };
    loadedImageHandle = gpu->createTexture(textureCI);

    if (loadedImageHandle.isValid()) {
      VkCommandBuffer cmd = gpu->getCommandBuffer();
      VkHelper::transitionImage(cmd, gpu->getTexture(skyboxHandle)->image,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      VkHelper::transitionImage(
          cmd, gpu->getTexture(irradianceMapHandle)->image,
          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      VkHelper::transitionImage(
          cmd, gpu->getTexture(prefilteredCubeHandle)->image,
          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      VkHelper::transitionImage(cmd, gpu->getTexture(brdfLutHandle)->image,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      gpu->submitImmediate(cmd);

      renderFacesOffscreenAndCopyToCubemap(
          cubemapPipelineHandle, skyboxHandle,
          {.data0 = loadedImageHandle.index, .data1 = samplerHandle.index});

      renderFacesOffscreenAndCopyToCubemap(
          irradianceMapPipelineHandle, irradianceMapHandle,
          {.data0 = skyboxHandle.index, .data1 = samplerHandle.index});

      renderFacesOffscreenAndCopyToCubemap(
          prefilteredCubePipelineHandle, prefilteredCubeHandle,
          {.data0 = skyboxHandle.index, .data1 = samplerHandle.index});
      getBrdfLut();

      spdlog::info("Skybox: loaded {}", path.filename().string());
      loaded = true;
    }

    free(stbData);
  }
}

void SkyboxPass::render(VkCommandBuffer cmd) {
  Pipeline *skyboxPipeline = gpu->getPipeline(skyboxPipelineHandle);

  VkRenderingAttachmentInfo colorAttachment = VkHelper::colorAttachment(
      gpu->getTexture(colorAttachmentHandle)->imageView,
      VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE);
  VkRenderingAttachmentInfo depthAttachment = VkHelper::depthAttachment(
      gpu->getTexture(depthAttachmentHandle)->imageView,
      VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE);
  VkRenderingInfo renderingInfo = VkHelper::renderingInfo(
      gpu->swapchainExtent, 1, &colorAttachment, &depthAttachment);

  vkCmdBeginRendering(cmd, &renderingInfo);

  vkCmdBindPipeline(cmd, skyboxPipeline->bindPoint, skyboxPipeline->pipeline);
  vkCmdBindDescriptorSets(cmd, skyboxPipeline->bindPoint,
                          skyboxPipeline->pipelineLayout, 0,
                          gpu->bindlessDescriptorSets.size(),
                          gpu->bindlessDescriptorSets.data(), 0, nullptr);
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(cmd, 0, 1, &gpu->getBuffer(vertexBufferHandle)->buffer,
                         offsets);
  vkCmdBindIndexBuffer(cmd, gpu->getBuffer(indexBufferHandle)->buffer, 0,
                       VK_INDEX_TYPE_UINT32);
  vkCmdPushConstants(cmd, skyboxPipeline->pipelineLayout, VK_SHADER_STAGE_ALL,
                     0, sizeof(PushConstants), &pc);
  vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);

  vkCmdEndRendering(cmd);
}

void SkyboxPass::renderFacesOffscreenAndCopyToCubemap(
    Handle<Pipeline> pipelineHandle, Handle<Texture> targetHandle,
    PushConstants pc) {
  Pipeline *pipeline = gpu->getPipeline(pipelineHandle);
  Texture *targetTexture = gpu->getTexture(targetHandle);

  VkRenderingAttachmentInfo colorAttachment = VkHelper::colorAttachment(
      gpu->getTexture(offscreenImageHandle)->imageView);

  VkRenderingInfo renderInfo = VkHelper::renderingInfo(
      SKYBOX_RESOLUTION, SKYBOX_RESOLUTION, 1, &colorAttachment);

  VkViewport viewport =
      VkHelper::viewport(SKYBOX_RESOLUTION, SKYBOX_RESOLUTION);
  VkRect2D scissor = VkHelper::scissor(SKYBOX_RESOLUTION, SKYBOX_RESOLUTION);

  for (size_t i = 0; i < faceMatrices.size(); i++) {
    pc.mat = glm::perspective((float)(std::numbers::pi / 2.0), 1.0f, 0.1f,
                              (float)(SKYBOX_RESOLUTION)) *
             faceMatrices[i];

    VkCommandBuffer cmd = gpu->getCommandBuffer();

    VkHelper::transitionImage(cmd, gpu->getTexture(offscreenImageHandle)->image,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    vkCmdBeginRendering(cmd, &renderInfo);

    vkCmdBindPipeline(cmd, pipeline->bindPoint, pipeline->pipeline);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindDescriptorSets(cmd, pipeline->bindPoint, pipeline->pipelineLayout,
                            0, gpu->bindlessDescriptorSets.size(),
                            gpu->bindlessDescriptorSets.data(), 0, nullptr);
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(
        cmd, 0, 1, &gpu->getBuffer(vertexBufferHandle)->buffer, offsets);
    vkCmdBindIndexBuffer(cmd, gpu->getBuffer(indexBufferHandle)->buffer, 0,
                         VK_INDEX_TYPE_UINT32);
    vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_ALL, 0,
                       sizeof(PushConstants), &pc);
    vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);
    vkCmdEndRendering(cmd);

    VkHelper::transitionImage(cmd, gpu->getTexture(offscreenImageHandle)->image,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    VkImageCopy copyRegion = {};
    copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.srcSubresource.baseArrayLayer = 0;
    copyRegion.srcSubresource.mipLevel = 0;
    copyRegion.srcSubresource.layerCount = 1;
    copyRegion.srcOffset = {0, 0, 0};

    copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.dstSubresource.baseArrayLayer = i;
    copyRegion.dstSubresource.mipLevel = 0;
    copyRegion.dstSubresource.layerCount = 1;
    copyRegion.dstOffset = {0, 0, 0};

    copyRegion.extent.width = SKYBOX_RESOLUTION;
    copyRegion.extent.height = SKYBOX_RESOLUTION;
    copyRegion.extent.depth = 1;

    vkCmdCopyImage(cmd, gpu->getTexture(offscreenImageHandle)->image,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, targetTexture->image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    gpu->submitImmediate(cmd);
  }

  VkCommandBuffer cmd = gpu->getCommandBuffer();
  VkHelper::genCubemapMips(cmd, targetTexture->image,
                           {targetTexture->width, targetTexture->height});
  gpu->submitImmediate(cmd);
}

void SkyboxPass::getBrdfLut() {
  // todo: save and load brdf lut from file if exists

  Pipeline *brdfLutPipeline = gpu->getPipeline(brdfLutPipelineHandle);

  VkViewport viewport =
      VkHelper::viewport(SKYBOX_RESOLUTION, SKYBOX_RESOLUTION);
  VkRect2D scissor = VkHelper::scissor(SKYBOX_RESOLUTION, SKYBOX_RESOLUTION);

  VkRenderingAttachmentInfo colorAttachment = VkHelper::colorAttachment(
      gpu->getTexture(offscreenImageHandle)->imageView);

  VkRenderingInfo renderInfo = VkHelper::renderingInfo(
      SKYBOX_RESOLUTION, SKYBOX_RESOLUTION, 1, &colorAttachment);

  VkCommandBuffer cmd = gpu->getCommandBuffer();

  VkHelper::transitionImage(cmd, gpu->getTexture(offscreenImageHandle)->image,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  vkCmdBeginRendering(cmd, &renderInfo);

  vkCmdBindPipeline(cmd, brdfLutPipeline->bindPoint, brdfLutPipeline->pipeline);
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &scissor);
  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRendering(cmd);

  VkHelper::transitionImage(cmd, gpu->getTexture(offscreenImageHandle)->image,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  VkImageCopy copyRegion = {};
  copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copyRegion.srcSubresource.baseArrayLayer = 0;
  copyRegion.srcSubresource.mipLevel = 0;
  copyRegion.srcSubresource.layerCount = 1;
  copyRegion.srcOffset = {0, 0, 0};

  copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copyRegion.dstSubresource.baseArrayLayer = 0;
  copyRegion.dstSubresource.mipLevel = 0;
  copyRegion.dstSubresource.layerCount = 1;
  copyRegion.dstOffset = {0, 0, 0};

  copyRegion.extent.width = SKYBOX_RESOLUTION;
  copyRegion.extent.height = SKYBOX_RESOLUTION;
  copyRegion.extent.depth = 1;

  vkCmdCopyImage(cmd, gpu->getTexture(offscreenImageHandle)->image,
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 gpu->getTexture(brdfLutHandle)->image,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

  gpu->submitImmediate(cmd);
}

void SkyboxPass::setInputs(const SkyboxInputs &inputs) {
  glm::mat4 fixedView = glm::mat4(glm::mat3(inputs.view));
  pc.mat = inputs.projection * fixedView;
  pc.data0 = skyboxHandle.index;
  pc.data1 = samplerHandle.index;

  colorAttachmentHandle = inputs.colorAttachment;
  depthAttachmentHandle = inputs.depthAttachment;
}
} // namespace Flare
