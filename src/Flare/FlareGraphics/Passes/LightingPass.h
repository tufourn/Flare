#pragma once

#include "../GpuResources.h"
#include "../RingBuffer.h"

namespace Flare {
struct GpuDevice;

struct LightingPassInputs {
  Handle<Texture> drawTexture;

  Handle<Buffer> cameraBuffer;
  Handle<Buffer> lightBuffer;

  Handle<Texture> gBufferAlbedo;
  Handle<Texture> gBufferNormal;
  Handle<Texture> gBufferOcclusionMetallicRoughness;
  Handle<Texture> gBufferEmissive;
  Handle<Texture> gBufferDepth;

  Handle<Texture> shadowMap;
  Handle<Sampler> shadowSampler;

  Handle<Texture> irradianceMap;
  Handle<Texture> prefilteredCube;
  Handle<Texture> brdfLut;
};

// stores indices of input textures
struct LightingPassUniform {
  uint32_t gBufferAlbedoIndex;
  uint32_t gBufferNormalIndex;
  uint32_t gBufferOcclusionMetallicRoughnessIndex;
  uint32_t gBufferEmissiveIndex;
  uint32_t gBufferDepthIndex;

  uint32_t shadowMapIndex;
  uint32_t shadowSamplerIndex;

  uint32_t irradianceMapIndex;
  uint32_t prefilteredCubeIndex;
  uint32_t brdfLutIndex;
};

struct LightingPass {
  void init(GpuDevice *gpuDevice);

  void shutdown();

  void render(VkCommandBuffer cmd);

  void setInputs(const LightingPassInputs &inputs);

  GpuDevice *gpu = nullptr;

  PipelineCI pipelineCI;
  Handle<Pipeline> pipelineHandle;
  Handle<Texture> targetHandle;

  PushConstants pc{};

  LightingPassUniform uniforms;
  RingBuffer uniformRingBuffer;

  bool loaded = false;
};
} // namespace Flare
