#pragma once

#include "GpuDevice.h"
#include <cgltf.h>
#include <filesystem>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/quaternion.hpp>

namespace Flare {
static constexpr uint32_t DEFAULT_ALBEDO_BASE_OFFSET = 1;
static constexpr uint32_t DEFAULT_METALLIC_ROUGHNESS_BASE_OFFSET = 2;
static constexpr uint32_t DEFAULT_NORMAL_BASE_OFFSET = 3;
static constexpr uint32_t DEFAULT_OCCLUSION_BASE_OFFSET = 4;
static constexpr uint32_t DEFAULT_EMISSIVE_BASE_OFFSET = 5;

struct TextureIndex {
  uint32_t imageIndex;
  uint32_t samplerIndex;
};

struct MeshDraw {
  uint32_t id = 0;
  uint32_t indexCount = 0;
  uint32_t indexOffset = 0;
  uint32_t vertexOffset = 0;
  uint32_t transformOffset = 0;
  uint32_t materialOffset = 0;

  Bounds bounds;
};

struct SkinData { // todo
  uint32_t positionOffset = 0;
};

struct Material {
  glm::vec4 albedoFactor;

  glm::vec3 emissiveFactor;
  uint32_t emissiveTextureOffset = 0;

  uint32_t albedoTextureOffset = 0;
  uint32_t metallicRoughnessTextureOffset = 0;
  uint32_t normalTextureOffset = 0;
  uint32_t occlusionTextureOffset = 0;

  float metallicFactor = 1.f;
  float roughnessFactor = 1.f;
  float alphaCutoff = 0.5f;
  float pad;
};

struct GltfMeshPrimitive {
  uint32_t id;
  std::vector<glm::vec4> positions;
  std::vector<uint32_t> indices;
  std::vector<glm::vec4> normals;
  std::vector<glm::vec2> uvs;
  std::vector<glm::vec4> tangents;
  uint32_t materialOffset;
  Bounds bounds;
};

struct GltfMesh {
  std::vector<GltfMeshPrimitive> meshPrimitives;
};

struct Node {
  Node *parent = nullptr;
  std::vector<Node *> children;

  GltfMesh *mesh = nullptr;

  glm::mat4 worldTransform = glm::mat4(1.f);

  glm::mat4 matrix = glm::mat4(1.f);
  glm::vec3 translation;
  glm::quat rotation;
  glm::vec3 scale;

  int skin = -1; // todo

  glm::mat4 getLocalTransform() const;

  void updateWorldTransform();
};

struct GltfScene {
  std::vector<Handle<Texture>> images;
  std::vector<Handle<Sampler>> samplers;
  std::vector<TextureIndex> gltfTextures;

  std::vector<Node> nodes;
  std::vector<Node *> topLevelNodes;
  std::vector<GltfMesh> meshes;
  std::vector<Material> materials;
  std::vector<glm::vec4> positions;
  std::vector<uint32_t> indices;
  std::vector<glm::vec2> uvs;
  std::vector<glm::vec4> normals;
  std::vector<glm::vec4> tangents;
  std::vector<glm::mat4> transforms;

  std::vector<MeshDraw> meshDraws;

  GpuDevice *gpu = nullptr;
  cgltf_data *data = nullptr;

  std::string filename;

  void init(const std::filesystem::path &path, GpuDevice *gpuDevice);

  void shutdown();

  void generateMeshDrawsFromNode(
      Node *node, std::unordered_map<uint32_t, std::vector<MeshDraw>> &map);

  [[nodiscard]] std::vector<MeshDraw> generateMeshDraws();
};
} // namespace Flare
