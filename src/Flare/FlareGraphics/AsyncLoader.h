#pragma once

#include <filesystem>

#include "GpuDevice.h"

namespace Flare {
struct UploadRequest {
  Texture *texture = nullptr;
  Buffer *dstBuffer = nullptr;
  Buffer *srcBuffer = nullptr;
  //        Handle<Texture> texture;
  //        Handle<Buffer> dstBuffer;
  //        Handle<Buffer> srcBuffer;
  void *data = nullptr;
};

struct FileRequest {
  std::filesystem::path path;
  Texture *texture;
  //        Handle<Texture> texture;
};

struct AsyncLoader {
  GpuDevice *gpu;

  std::array<VkCommandPool, FRAMES_IN_FLIGHT> commandPools;
  std::array<VkCommandBuffer, FRAMES_IN_FLIGHT> commandBuffers;

  VkFence transferFence = VK_NULL_HANDLE;

  Handle<Buffer> stagingBufferHandle;
  std::atomic_size_t stagingBufferOffset = 0;

  std::vector<UploadRequest> uploadRequests;
  std::vector<FileRequest> fileRequests;

  std::mutex textureMutex;
  std::vector<Texture *> pendingTextures;

  std::mutex bufferMutex;
  std::vector<Buffer *> pendingBuffers;

  void init(GpuDevice &gpuDevice);

  void shutdown();

  void update();

  void signalTextures(VkCommandBuffer cmd);

  void signalBuffers(VkCommandBuffer cmd);
};
} // namespace Flare
