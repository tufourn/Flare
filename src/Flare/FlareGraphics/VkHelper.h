#pragma once

#include <volk.h>

namespace VkHelper {
    void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout initialLayout, VkImageLayout finalLayout);

    VkComponentMapping identityRGBA();

    VkImageSubresourceRange subresourceRange(bool depth = false);

    VkViewport viewport(float width, float height);

    VkRect2D scissor(float width, float height);

    size_t memoryAlign(size_t size, size_t alignment);

    VkFilter extractGltfMagFilter(int gltfMagFilter);

    VkFilter extractGltfMinFilter(int gltfMinFilter);

    VkSamplerAddressMode extractGltfWrapMode(int gltfWrap);

    VkSamplerMipmapMode extractGltfMipmapMode(int gltfMinFilter);

    uint32_t getMipLevel(uint32_t width, uint32_t height);

    void genMips(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);

    void genCubemapMips(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);
}

