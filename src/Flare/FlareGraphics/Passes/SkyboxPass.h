#pragma once

#include "../GpuResources.h"

#include <array>
#include <glm/ext/matrix_transform.hpp>

namespace Flare {
struct GpuDevice;

static constexpr uint32_t SKYBOX_RESOLUTION = 1024;

static const std::array<glm::mat4, 6> faceMatrices = {
    // POSITIVE_X
    glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f),
                            glm::vec3(0.0f, 1.0f, 0.0f)),
                glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
    // NEGATIVE_X
    glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f),
                            glm::vec3(0.0f, 1.0f, 0.0f)),
                glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
    // POSITIVE_Y
    glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f),
                glm::vec3(1.0f, 0.0f, 0.0f)),
    // NEGATIVE_Y
    glm::rotate(glm::mat4(1.0f), glm::radians(90.0f),
                glm::vec3(1.0f, 0.0f, 0.0f)),
    // POSITIVE_Z
    glm::rotate(glm::mat4(1.0f), glm::radians(180.0f),
                glm::vec3(1.0f, 0.0f, 0.0f)),
    // NEGATIVE_Z
    glm::rotate(glm::mat4(1.0f), glm::radians(180.0f),
                glm::vec3(0.0f, 0.0f, 1.0f)),
};

struct SkyboxInputs {
  glm::mat4 projection;
  glm::mat4 view;
  Handle<Texture> colorAttachment;
  Handle<Texture> depthAttachment;
};

struct SkyboxPass {
  void init(GpuDevice *gpuDevice);

  void shutdown();

  void loadImage(std::filesystem::path path);

  void render(VkCommandBuffer cmd);

  void setInputs(const SkyboxInputs &inputs);

  void renderFacesOffscreenAndCopyToCubemap(Handle<Pipeline> pipelineHandle,
                                            Handle<Texture> targetHandle,
                                            PushConstants pc);

  void getBrdfLut();

  GpuDevice *gpu;

  bool loaded = false;

  Handle<Texture> loadedImageHandle;
  Handle<Texture> skyboxHandle;
  Handle<Texture> irradianceMapHandle;
  Handle<Texture> prefilteredCubeHandle;
  Handle<Texture> brdfLutHandle;
  Handle<Texture> offscreenImageHandle;

  Handle<Sampler> samplerHandle;

  Handle<Buffer> vertexBufferHandle;
  Handle<Buffer> indexBufferHandle;

  Handle<Pipeline> cubemapPipelineHandle;
  Handle<Pipeline> skyboxPipelineHandle;
  Handle<Pipeline> irradianceMapPipelineHandle;
  Handle<Pipeline> prefilteredCubePipelineHandle;
  Handle<Pipeline> brdfLutPipelineHandle;

  Handle<Texture> colorAttachmentHandle;
  Handle<Texture> depthAttachmentHandle;
  PushConstants pc;
};
} // namespace Flare
