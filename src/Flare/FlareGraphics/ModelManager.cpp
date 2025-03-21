#include "ModelManager.h"
#include "GpuDevice.h"
#include "ImGuiFileDialog.h"

#include <imgui.h>

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
ModelManager::addInstance(Handle<ModelPrefab> prefabHandle) {
  Handle<ModelInstance> handle = modelInstances.obtain();

  ModelInstance *instance = modelInstances.get(handle);
  loadedInstances.push_back(handle);

  instance->prefabHandle = prefabHandle;

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
  if (!queuedPrefabPaths.empty()) {
    std::filesystem::path path = queuedPrefabPaths.back();
    queuedPrefabPaths.pop_back();

    loadPrefab(path);
  }

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

      glm::mat4 transform = glm::mat4(1.0f);
      transform = glm::translate(transform, instance->translation);
      transform = glm::rotate(transform, glm::radians(instance->rotation.x),
                              glm::vec3(1.f, 0.f, 0.f));
      transform = glm::rotate(transform, glm::radians(instance->rotation.y),
                              glm::vec3(0.f, 1.f, 0.f));
      transform = glm::rotate(transform, glm::radians(instance->rotation.z),
                              glm::vec3(0.f, 0.f, 1.f));
      transform = glm::scale(transform, instance->scale);
      transform =
          transform * prefab->gltfModel.transforms[meshDraw.transformOffset];

      transforms.push_back(transform);
      bounds.push_back(meshDraw.bounds);
    }
  }

  if (!transforms.empty() &&
      (!transformRingBuffer.buffer().isValid() ||
       transforms.size() * sizeof(glm::mat4) >
           gpu->getBuffer(transformRingBuffer.buffer())->size)) {
    BufferCI transformsCI = {
        .size = transforms.size() * sizeof(glm::mat4),
        .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .mapped = true,
        .name = "transforms",
    };
    transformRingBuffer.createBuffer(transformsCI);
  }

  if (!indirectDrawDatas.empty() &&
      (!indirectDrawDataRingBuffer.buffer().isValid() ||
       indirectDrawDatas.size() * sizeof(IndirectDrawData) >
           gpu->getBuffer(indirectDrawDataRingBuffer.buffer())->size)) {
    BufferCI indirectDrawsCI = {
        .size = sizeof(IndirectDrawData) * indirectDrawDatas.size(),
        .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                      VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        .mapped = true,
        .name = "indirect draws",
    };
    indirectDrawDataRingBuffer.createBuffer(indirectDrawsCI);
  }

  if (!bounds.empty() &&
      (!boundsRingBuffer.buffer().isValid() ||
       bounds.size() * sizeof(Bounds) >
           gpu->getBuffer(boundsRingBuffer.buffer())->size)) {
    BufferCI boundsCI = {
        .size = sizeof(Bounds) * bounds.size(),
        .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .mapped = true,
        .name = "bounds",
    };
    boundsRingBuffer.createBuffer(boundsCI);
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
  memcpy(gpu->getBuffer(countRingBuffer.buffer())->allocationInfo.pMappedData,
         &count, sizeof(uint32_t));
}
void ModelManager::drawImguiMenu() {
  ImGui::Begin("Model Manager");

  if (ImGui::Button("Load gltf prefab")) {
    IGFD::FileDialogConfig config;
    config.path = ".";
    ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File",
                                            ".gltf,.glb", config);
  }

  if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      queuedPrefabPaths.push_back(filePathName);
    }
    ImGuiFileDialog::Instance()->Close();
  }

  std::vector<std::string> prefabNames;
  std::vector<Handle<ModelPrefab>> prefabHandles;

  prefabNames.reserve(loadedPrefabs.size());
  prefabHandles.reserve(loadedPrefabs.size());
  for (const auto &[path, prefabHandle] : loadedPrefabs) {
    prefabHandles.push_back(prefabHandle);
    prefabNames.push_back(getPrefab(prefabHandle)->gltfModel.filename);
  }

  if (ImGui::BeginListBox("Prefabs")) {
    for (size_t n = 0; n < prefabNames.size(); n++) {
      const bool isSelected = (selectedPrefabIndex == n);
      if (ImGui::Selectable(prefabNames[n].c_str(), isSelected)) {
        selectedPrefabIndex = n;
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndListBox();
  }

  if (selectedPrefabIndex >= 0) {
    ImGui::Text("Selected prefab %s", prefabNames[selectedPrefabIndex].c_str());
    if (ImGui::Button("Add instance")) {
      addInstance(prefabHandles[selectedPrefabIndex]);
    }
  }

  std::vector<std::string> instanceNames(loadedInstances.size());
  for (size_t i = 0; i < loadedInstances.size(); i++) {
    instanceNames[i] = "Instance " + std::to_string(i);
  }
  if (ImGui::BeginListBox("Instances")) {
    for (size_t n = 0; n < instanceNames.size(); n++) {
      const bool isSelected = (selectedInstanceIndex == n);
      if (ImGui::Selectable(instanceNames[n].c_str(), isSelected)) {
        selectedInstanceIndex = n;
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndListBox();
  }
  if (selectedInstanceIndex >= 0) {
    ModelInstance *instance =
        getInstance(loadedInstances[selectedInstanceIndex]);

    ImGui::Text("Selected instance %d", selectedInstanceIndex);
    ImGui::SliderFloat3("Translation",
                        reinterpret_cast<float *>(&instance->translation),
                        -10.f, 10.f);
    ImGui::SliderFloat3(
        "Rotation", reinterpret_cast<float *>(&instance->rotation), 0.f, 360.f);
    ImGui::SliderFloat3("Scale", reinterpret_cast<float *>(&instance->scale),
                        0.f, 5.f);
    if (ImGui::Button("Remove instance")) {
      loadedInstances.erase(loadedInstances.begin() + selectedInstanceIndex);
      selectedInstanceIndex = -1;
    }
  }

  ImGui::End();
}
} // namespace Flare