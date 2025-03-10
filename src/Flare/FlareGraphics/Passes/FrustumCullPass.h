#pragma once

#include "../GpuResources.h"
#include "../RingBuffer.h"

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

  PipelineCI pipelineCI;
  Handle<Pipeline> pipelineHandle;

  FrustumCullUniforms uniforms;
  RingBuffer frustumUniformRingBuffer;
  PushConstants pc;

  uint32_t maxDrawCount = 0;
  Handle<Buffer>
      outputIndirectBufferHandle; // store these 2 handles to add barrier
  Handle<Buffer> outputCountBufferHandle;
};
} // namespace Flare
