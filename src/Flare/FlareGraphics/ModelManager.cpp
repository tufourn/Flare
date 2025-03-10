#include "ModelManager.h"
#include "GpuDevice.h"

namespace Flare {

void ModelManager::init(GpuDevice *gpuDevice, uint32_t prefabCount,
                        uint32_t instanceCount) {
  gpu = gpuDevice;

  transformRingBuffer.init(gpu, FRAMES_IN_FLIGHT);
  indirectDrawDataRingBuffer.init(gpu, FRAMES_IN_FLIGHT);
  boundsRingBuffer.init(gpu, FRAMES_IN_FLIGHT);

  modelPrefabs.init(prefabCount);
  modelInstances.init(instanceCount);

  BufferCI countCI = {.size = sizeof(uint32_t),
                      .usageFlags = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                      .mapped = true,
                      .name = "count"};
  countRingBuffer.init(gpu, FRAMES_IN_FLIGHT, countCI);
}

void ModelManager::shutdown() {
  std::vector<Handle<ModelPrefab>> handles;
  handles.reserve(loadedPrefabs.size());
  for (auto &[path, handle] : loadedPrefabs) {
    handles.push_back(handle);
  }

  for (auto &handle : handles) {
    removePrefab(handle);
  }

  destroyModelBuffers();

  transformRingBuffer.shutdown();
  indirectDrawDataRingBuffer.shutdown();
  boundsRingBuffer.shutdown();
  countRingBuffer.shutdown();
}

Handle<ModelPrefab> ModelManager::loadPrefab(std::filesystem::path path) {
  if (loadedPrefabs.contains(path)) {
    spdlog::info("{} already loaded, reusing", path.string());
    return loadedPrefabs.at(path);
  }

  Handle<ModelPrefab> handle = modelPrefabs.obtain();
  loadedPrefabs.insert({path, handle});

  ModelPrefab *modelPrefab = modelPrefabs.get(handle);

  GltfScene &gltf = modelPrefab->gltfModel;
  gltf.init(path, gpu);

  modelPrefab->indexOffset = indices.size();
  modelPrefab->vertexOffset = positions.size();
  modelPrefab->materialOffset = materials.size();
  modelPrefab->transformOffset = transforms.size();

  indices.insert(indices.end(), gltf.indices.begin(), gltf.indices.end());
  positions.insert(positions.end(), gltf.positions.begin(),
                   gltf.positions.end());
  normals.insert(normals.end(), gltf.normals.begin(), gltf.normals.end());
  tangents.insert(tangents.end(), gltf.tangents.begin(), gltf.tangents.end());
  uvs.insert(uvs.end(), gltf.uvs.begin(), gltf.uvs.end());
  materials.reserve(materials.size() + gltf.materials.size());
  for (const auto &material : gltf.materials) {
    Material mat = material;
    mat.albedoTextureOffset += textureIndices.size();
    mat.metallicRoughnessTextureOffset += textureIndices.size();
    mat.normalTextureOffset += textureIndices.size();
    mat.occlusionTextureOffset += textureIndices.size();
    mat.emissiveTextureOffset += textureIndices.size();
    materials.push_back(mat);
  }
  textureIndices.insert(textureIndices.end(), gltf.gltfTextures.begin(),
                        gltf.gltfTextures.end());

  buildBuffers();

  spdlog::info("loaded prefab {}", path.string());
  return handle;
}

void ModelManager::removePrefab(Handle<ModelPrefab> handle) {
  if (!handle.isValid()) {
    spdlog::error("Invalid gltf handle");
    return;
  }

  bool found = false;
  for (auto it = loadedPrefabs.begin(); it != loadedPrefabs.end(); it++) {
    if (it->second == handle) {
      ModelPrefab *modelPrefab = modelPrefabs.get(handle);
      modelPrefab->gltfModel.shutdown();

      loadedPrefabs.erase(it);
      modelPrefabs.release(handle);

      found = true;
      break;
    }
  }

  if (!found) {
    spdlog::error("prefab handle not found");
  }
}
Handle<ModelInstance>
ModelManager::addInstance(Handle<ModelPrefab> prefabHandle,
                          glm::mat4 transform) {
  Handle<ModelInstance> handle = modelInstances.obtain();

  ModelInstance *instance = modelInstances.get(handle);
  loadedInstances.push_back(handle);

  instance->prefabHandle = prefabHandle;
  instance->modelTransform = transform;

  return handle;
}
void ModelManager::destroyModelBuffers() {
  vkDeviceWaitIdle(gpu->device); // TODO: sync

  if (indexBufferHandle.isValid()) {
    gpu->destroyBuffer(indexBufferHandle);
  }
  if (positionBufferHandle.isValid()) {
    gpu->destroyBuffer(positionBufferHandle);
  }
  if (normalBufferHandle.isValid()) {
    gpu->destroyBuffer(normalBufferHandle);
  }
  if (tangentBufferHandle.isValid()) {
    gpu->destroyBuffer(tangentBufferHandle);
  }
  if (uvBufferHandle.isValid()) {
    gpu->destroyBuffer(uvBufferHandle);
  }
  if (textureIndexBufferHandle.isValid()) {
    gpu->destroyBuffer(textureIndexBufferHandle);
  }
  if (materialBufferHandle.isValid()) {
    gpu->destroyBuffer(materialBufferHandle);
  }
}
void ModelManager::buildBuffers() {
  destroyModelBuffers();

  BufferCI indicesCI = {
      .initialData = indices.data(),
      .size = sizeof(uint32_t) * indices.size(),
      .usageFlags =
          VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      .name = "indices",
  };
  indexBufferHandle = gpu->createBuffer(indicesCI);

  BufferCI positionsCI = {
      .initialData = positions.data(),
      .size = sizeof(glm::vec4) * positions.size(),
      .usageFlags =
          VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      .name = "positions",
  };
  positionBufferHandle = gpu->createBuffer(positionsCI);

  BufferCI normalsCI = {
      .initialData = normals.data(),
      .size = sizeof(glm::vec4) * normals.size(),
      .usageFlags =
          VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      .name = "normals",
  };
  normalBufferHandle = gpu->createBuffer(normalsCI);

  BufferCI tangentsCI = {
      .initialData = tangents.data(),
      .size = sizeof(glm::vec4) * tangents.size(),
      .usageFlags =
          VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      .name = "tangents",
  };
  tangentBufferHandle = gpu->createBuffer(tangentsCI);

  BufferCI uvsCI = {
      .initialData = uvs.data(),
      .size = sizeof(glm::vec2) * uvs.size(),
      .usageFlags =
          VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      .name = "uv",
  };
  uvBufferHandle = gpu->createBuffer(uvsCI);

  BufferCI textureIndicesCI = {
      .initialData = textureIndices.data(),
      .size = sizeof(TextureIndex) * textureIndices.size(),
      .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .name = "textures",
  };
  textureIndexBufferHandle = gpu->createBuffer(textureIndicesCI);

  BufferCI materialsCI = {
      .initialData = materials.data(),
      .size = sizeof(Material) * materials.size(),
      .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .name = "materials",
  };
  materialBufferHandle = gpu->createBuffer(materialsCI);
}

void ModelManager::newFrame() {
  indirectDrawDataRingBuffer.moveToNextBuffer();
  indirectDrawDatas.clear();

  transformRingBuffer.moveToNextBuffer();
  transforms.clear();

  boundsRingBuffer.moveToNextBuffer();
  bounds.clear();

  countRingBuffer.moveToNextBuffer();

  for (const auto &instanceHandle : loadedInstances) {
    const ModelInstance *instance = modelInstances.get(instanceHandle);
    const ModelPrefab *prefab = modelPrefabs.get(instance->prefabHandle);

    for (const auto &meshDraw : prefab->gltfModel.meshDraws) {
      IndirectDrawData indirectDrawData = {
          .cmd =
              {
                  .indexCount = meshDraw.indexCount,
                  .instanceCount = 1, // todo: instanced draw?
                  .firstIndex = meshDraw.indexOffset + prefab->indexOffset,
                  .vertexOffset = static_cast<int32_t>(meshDraw.vertexOffset +
                                                       prefab->vertexOffset),
                  .firstInstance = 0,
              },
          .materialOffset = meshDraw.materialOffset + prefab->materialOffset,
          .transformOffset = static_cast<uint32_t>(transforms.size()),
      };

      indirectDrawDatas.push_back(indirectDrawData);

      glm::mat4 transform =
          instance->modelTransform *
          prefab->gltfModel.transforms[meshDraw.transformOffset];
      transforms.push_back(transform);

      Bounds newBounds = meshDraw.bounds;
      newBounds.origin = transform * glm::vec4(newBounds.origin, 1.f);
      newBounds.radius *=
          std::max(std::max(transform[0][0], transform[1][1]), transform[2][2]);
      newBounds.extents *=
          std::max(std::max(transform[0][0], transform[1][1]), transform[2][2]);
      bounds.push_back(newBounds);
    }
  }

  if (!transforms.empty() &&
      (!transformRingBuffer.buffer().isValid() ||
       transforms.size() * sizeof(glm::mat4) >
           gpu->getBuffer(transformRingBuffer.buffer())->size)) {
    if (transformRingBuffer.buffer().isValid()) {
      gpu->destroyBuffer(transformRingBuffer.buffer());
    }

    BufferCI transformsCI = {
        .size = transforms.size() * sizeof(glm::mat4),
        .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .mapped = true,
        .name = "transforms",
    };

    transformRingBuffer.bufferRing[transformRingBuffer.ringIndex] =
        gpu->createBuffer(transformsCI);
  }

  if (!indirectDrawDatas.empty() &&
      (!indirectDrawDataRingBuffer.buffer().isValid() ||
       indirectDrawDatas.size() * sizeof(IndirectDrawData) >
           gpu->getBuffer(indirectDrawDataRingBuffer.buffer())->size)) {
    if (indirectDrawDataRingBuffer.buffer().isValid()) {
      gpu->destroyBuffer(indirectDrawDataRingBuffer.buffer());
    }

    BufferCI indirectDrawsCI = {
        .size = sizeof(IndirectDrawData) * indirectDrawDatas.size(),
        .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                      VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        .mapped = true,
        .name = "indirect draws",
    };
    indirectDrawDataRingBuffer
        .bufferRing[indirectDrawDataRingBuffer.ringIndex] =
        gpu->createBuffer(indirectDrawsCI);
  }
  if (!bounds.empty() &&
      (!boundsRingBuffer.buffer().isValid() ||
       bounds.size() * sizeof(Bounds) >
           gpu->getBuffer(boundsRingBuffer.buffer())->size)) {
    if (boundsRingBuffer.buffer().isValid()) {
      gpu->destroyBuffer(boundsRingBuffer.buffer());
    }

    BufferCI boundsCI = {
        .size = sizeof(Bounds) * bounds.size(),
        .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .mapped = true,
        .name = "bounds",
    };
    boundsRingBuffer.bufferRing[boundsRingBuffer.ringIndex] =
        gpu->createBuffer(boundsCI);
  }

  if (transformRingBuffer.buffer().isValid()) {
    memcpy(gpu->getBuffer(transformRingBuffer.buffer())
               ->allocationInfo.pMappedData,
           transforms.data(), transforms.size() * sizeof(glm::mat4));
  }

  if (indirectDrawDataRingBuffer.buffer().isValid()) {
    memcpy(gpu->getBuffer(indirectDrawDataRingBuffer.buffer())
               ->allocationInfo.pMappedData,
           indirectDrawDatas.data(),
           indirectDrawDatas.size() * sizeof(IndirectDrawData));
  }

  if (boundsRingBuffer.buffer().isValid()) {
    memcpy(
        gpu->getBuffer(boundsRingBuffer.buffer())->allocationInfo.pMappedData,
        bounds.data(), bounds.size() * sizeof(Bounds));
  }

  count = indirectDrawDatas.size();
}
} // namespace Flare