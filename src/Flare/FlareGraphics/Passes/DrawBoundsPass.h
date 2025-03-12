#pragma once

#include "../GpuResources.h"

#include <array>

namespace Flare {
struct GpuDevice;

constexpr std::array<glm::vec4, 8> boxCorners = {
    glm::vec4(-1, -1, -1, 1), // left bottom near
    glm::vec4(-1, 1, -1, 1),  // left top near
    glm::vec4(1, 1, -1, 1),   // right top near
    glm::vec4(1, -1, -1, 1),  // right bottom near
    glm::vec4(-1, -1, 1, 1),  // left bottom far
    glm::vec4(-1, 1, 1, 1),   // left top far
    glm::vec4(1, 1, 1, 1),    // right top far
    glm::vec4(1, -1, 1, 1),   // right bottom far
};

constexpr std::array<uint32_t, 24> boxIndices = {
    0, 1, 1, 2, 2, 3, 3, 0, // near
    4, 5, 5, 6, 6, 7, 7, 4, // far
    0, 4, 1, 5, 2, 6, 3, 7, // connecting near-far
};

struct DrawBoundsInputs {
  glm::mat4 viewProjection;
  Handle<Buffer> boundsBuffer;
  Handle<Buffer> transformBuffer;
  uint32_t count;
};

struct DrawBoundsPass {
  void init(GpuDevice *gpuDevice);

  void shutdown();

  void setInputs(const DrawBoundsInputs &inputs);

  void render(VkCommandBuffer cmd);

  GpuDevice *gpu;

  PipelineCI pipelineCI;
  Handle<Pipeline> pipelineHandle;

  Handle<Buffer> vertexBufferHandle;
  Handle<Buffer> indexBufferHandle;

  DrawBoundsInputs drawBoundsInputs;
};

} // namespace Flare
