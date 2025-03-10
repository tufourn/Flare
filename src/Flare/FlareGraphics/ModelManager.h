#pragma once

#include "GltfScene.h"
#include "GpuResources.h"
#include "RingBuffer.h"

namespace Flare {
struct GpuDevice;

struct ModelPrefab {
  GltfScene gltfModel;
  uint32_t indexOffset;
  uint32_t vertexOffset;
  uint32_t materialOffset;
  uint32_t transformOffset;
};

struct ModelInstance {
  glm::mat4 modelTransform;
  Handle<ModelPrefab> prefabHandle;
};

struct ModelManager {
  void init(GpuDevice *gpuDevice, uint32_t prefabCount, uint32_t instanceCount);
  void shutdown();

  [[nodiscard]] Handle<ModelPrefab> loadPrefab(std::filesystem::path path);

  void removePrefab(Handle<ModelPrefab> handle);

  Handle<ModelInstance> addInstance(Handle<ModelPrefab> prefabHandle,
                                    glm::mat4 transform);

  void destroyModelBuffers();

  void buildBuffers();

  void newFrame();

  GpuDevice *gpu;
  bool loaded = false;

  std::unordered_map<std::filesystem::path, Handle<ModelPrefab>> loadedPrefabs;
  ResourcePool<ModelPrefab> modelPrefabs;

  std::vector<Handle<ModelInstance>> loadedInstances;
  ResourcePool<ModelInstance> modelInstances;

  std::vector<uint32_t> indices;
  Handle<Buffer> indexBufferHandle;

  std::vector<glm::vec4> positions;
  Handle<Buffer> positionBufferHandle;

  std::vector<glm::vec4> normals;
  Handle<Buffer> normalBufferHandle;

  std::vector<glm::vec4> tangents;
  Handle<Buffer> tangentBufferHandle;

  std::vector<glm::vec2> uvs;
  Handle<Buffer> uvBufferHandle;

  std::vector<TextureIndex> textureIndices;
  Handle<Buffer> textureIndexBufferHandle;

  std::vector<Material> materials;
  Handle<Buffer> materialBufferHandle;

  std::vector<glm::mat4> transforms;
  RingBuffer transformRingBuffer;

  std::vector<IndirectDrawData> indirectDrawDatas;
  RingBuffer indirectDrawDataRingBuffer;

  std::vector<Bounds> bounds;
  RingBuffer boundsRingBuffer;

  uint32_t count;
  RingBuffer countRingBuffer;
};

} // namespace Flare
