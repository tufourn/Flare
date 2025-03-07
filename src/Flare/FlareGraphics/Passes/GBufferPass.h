#pragma once

#include "../GpuResources.h"
#include "../RingBuffer.h"

namespace Flare {
struct GpuDevice;

struct GBufferUniforms {
  glm::mat4 viewProjection = glm::mat4(1.f);
  glm::mat4 prevViewProjection = glm::mat4(1.f);
};

struct GBufferInputs {
  glm::mat4 viewProjection = glm::mat4(1.f);
  MeshDrawBuffers meshDrawBuffers;
};

struct GBufferPass {
  void init(GpuDevice *gpuDevice);

  void render(VkCommandBuffer cmd);

  void destroyRenderTargets();

  void shutdown();

  void generateRenderTargets();

  void setInputs(const GBufferInputs &inputs);

  GpuDevice *gpu = nullptr;

  bool loaded = false;

  Handle<Texture> depthTargetHandle;
  Handle<Texture> albedoTargetHandle;
  Handle<Texture> normalTargetHandle;
  Handle<Texture> occlusionMetallicRoughnessTargetHandle;
  Handle<Texture> emissiveTargetHandle;

  PipelineCI pipelineCI;
  Handle<Pipeline> pipelineHandle;

  MeshDrawBuffers meshDrawBuffers;

  PushConstants pc{};
  GBufferUniforms uniforms;
  RingBuffer gBufferUniformRingBuffer;
};
} // namespace Flare
