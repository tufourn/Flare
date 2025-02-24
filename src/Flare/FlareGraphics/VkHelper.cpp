#include "VkHelper.h"

namespace VkHelper {
    void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout initialLayout, VkImageLayout finalLayout) {
        // this uses all commands stage flag which is not optimal
        VkImageMemoryBarrier2 imageBarrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext = nullptr,
                .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
                .oldLayout = initialLayout,
                .newLayout = finalLayout,
                .image = image,
                .subresourceRange = subresourceRange(
                        initialLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
                        finalLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                )
        };

        VkDependencyInfo dependencyInfo = {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .pNext = nullptr,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &imageBarrier,
        };

        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }

    VkComponentMapping identityRGBA() {
        return {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        };
    }

    VkImageSubresourceRange subresourceRange(bool depth) {
        return {
                .aspectMask = static_cast<VkImageAspectFlags>(depth ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                                    : VK_IMAGE_ASPECT_COLOR_BIT),
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS
        };
    }

    size_t memoryAlign(size_t size, size_t alignment) {
        const size_t alignmentMask = alignment - 1;
        return (size + alignmentMask) & ~alignmentMask;
    }

    VkFilter extractGltfMagFilter(int gltfMagFilter) {
        switch (gltfMagFilter) {
            case 9728:
                return VK_FILTER_NEAREST;
            case 9729:
                return VK_FILTER_LINEAR;
            default:
                return VK_FILTER_LINEAR;
        }
    }

    VkFilter extractGltfMinFilter(int gltfMinFilter) {
        switch (gltfMinFilter) {
            case 9728:
                return VK_FILTER_NEAREST;
            case 9729:
                return VK_FILTER_LINEAR;
            case 9984:
                return VK_FILTER_NEAREST;
            case 9985:
                return VK_FILTER_NEAREST;
            case 9986:
                return VK_FILTER_LINEAR;
            case 9987:
                return VK_FILTER_LINEAR;
            default:
                return VK_FILTER_LINEAR;
        }
    }

    VkSamplerAddressMode extractGltfWrapMode(int gltfWrap) {
        switch (gltfWrap) {
            case 33071:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case 33648:
                return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            case 10497:
                return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            default:
                return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    }

    VkSamplerMipmapMode extractGltfMipmapMode(int gltfMinFilter) {
        switch (gltfMinFilter) {
            case 9984:
                return VK_SAMPLER_MIPMAP_MODE_NEAREST;
            case 9985:
                return VK_SAMPLER_MIPMAP_MODE_NEAREST;
            case 9986:
                return VK_SAMPLER_MIPMAP_MODE_LINEAR;
            case 9987:
                return VK_SAMPLER_MIPMAP_MODE_LINEAR;
            default:
                return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        }
    }
}
