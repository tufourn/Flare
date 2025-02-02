#pragma once

#include "GpuDevice.h"
#include <stb_image.h>

namespace Flare {
    struct UploadRequest {
        Handle<Texture> texture;
        Handle<Buffer> dstBuffer;
        Handle<Buffer> srcBuffer;
        void* data = nullptr;
    };

    struct FileRequest {
        std::filesystem::path path;
        Handle<Texture> texture;
    };

    struct AsyncLoader {
        GpuDevice* gpu;

        std::array<VkCommandPool, FRAMES_IN_FLIGHT> commandPools;
        std::array<VkCommandBuffer, FRAMES_IN_FLIGHT> commandBuffers;

        VkFence transferFence = VK_NULL_HANDLE;

        Handle<Buffer> stagingBufferHandle;
        std::atomic_size_t stagingBufferOffset = 0;

        std::vector<UploadRequest> uploadRequests;
        std::vector<FileRequest> fileRequests;

        Handle<Texture> loadedTexture;

        void init(GpuDevice& gpuDevice);
        void shutdown();
        void update();

        size_t memoryAlign(size_t size, size_t alignment);
    };
}
