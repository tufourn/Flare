#pragma once

#include <volk.h>

namespace VkHelper {
    void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout initialLayout, VkImageLayout finalLayout);

    VkComponentMapping identityRGBA();

    VkImageSubresourceRange subresourceRange(bool depth = false);

    size_t memoryAlign(size_t size, size_t alignment);

    VkFilter extractGltfMagFilter(int gltfMagFilter);

    VkFilter extractGltfMinFilter(int gltfMinFilter);

    VkSamplerAddressMode extractGltfWrapMode(int gltfWrap);

    VkSamplerMipmapMode extractGltfMipmapMode(int gltfMinFilter);

}

