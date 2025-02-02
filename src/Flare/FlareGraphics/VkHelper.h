#pragma once

#include <volk.h>

namespace VkHelper {
    void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout initialLayout, VkImageLayout finalLayout);

    VkComponentMapping identityRGBA();

    VkImageSubresourceRange subresourceRange(bool depth = false);
}

