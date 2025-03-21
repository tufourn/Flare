#pragma once

#include "../GpuResources.h"
#include "../RingBuffer.h"

#include <array>

namespace Flare {
struct GpuDevice;

struct FrustumPlanes {
  glm::vec4 left;
  glm::vec4 right;
  glm::vec4 bottom;
  glm::vec4 top;
  glm::vec4 near;
  glm::vec4 far;
};

struct FrustumCullUniforms {
  FrustumPlanes frustumPlanes;
};

struct FrustumCullInputs {
  glm::mat4 viewProjection;
  Handle<Buffer> inputIndirectDrawBuffer;
  Handle<Buffer> outputIndirectDrawBuffer;
  Handle<Buffer> countBuffer;
  Handle<Buffer> boundsBuffer;
  Handle<Buffer> transformBuffer;
  uint32_t maxDrawCount;
};

struct FrustumCullPass {
  void init(GpuDevice *gpuDevice);

  static FrustumPlanes getFrustumPlanes(glm::mat4 mat, bool normalize = true);

  void setInputs(const FrustumCullInputs &inputs);

  void cull(VkCommandBuffer cmd);

  void addBarriers(VkCommandBuffer cmd, uint32_t computeFamily,
                   uint32_t mainFamily);

  void shutdown();

  GpuDevice *gpu = nullptr;

  bool fixedFrustum = false;
  glm::mat4 viewProjection;

  PipelineCI pipelineCI;
  Handle<Pipeline> pipelineHandle;

  FrustumCullUniforms uniforms;
  RingBuffer frustumUniformRingBuffer;
  PushConstants pc;

  uint32_t maxDrawCount = 0;

  // store these 2 handles to add barrier
  Handle<Buffer> outputIndirectBufferHandle;
  Handle<Buffer> outputCountBufferHandle;
};
} // namespace Flare
