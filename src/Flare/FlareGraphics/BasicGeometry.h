#pragma once

#include <glm/glm.hpp>

namespace Flare {
struct MeshBuffers {
  std::vector<glm::vec4> positions;
  std::vector<glm::vec4> normals;
  std::vector<glm::vec4> tangents;
  std::vector<glm::vec2> uvs;
  std::vector<uint32_t> indices;
};

MeshBuffers createCubeMesh(float edgeX, float edgeY, float edgeZ);
} // namespace Flare
