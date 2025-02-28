#include <cmath>
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

    uint32_t getMipLevel(uint32_t width, uint32_t height) {
        return uint32_t(std::floor(std::log2(std::max(width, height)))) + 1;
    }

    void genMips(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize) {
        int mipLevels = int(std::floor(std::log2(std::max(imageSize.width, imageSize.height)))) + 1;

        for (size_t mip_i = 0; mip_i < mipLevels; mip_i++) {
            VkExtent2D halfSize = imageSize;
            halfSize.width /= 2;
            halfSize.height /= 2;

            VkImageMemoryBarrier2 imageBarrier = {};
            imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
            imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
            imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

            imageBarrier.subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = static_cast<uint32_t>(mip_i),
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
            };
            imageBarrier.image = image;

            VkDependencyInfo dependencyInfo = {};
            dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependencyInfo.imageMemoryBarrierCount = 1;
            dependencyInfo.pImageMemoryBarriers = &imageBarrier;

            vkCmdPipelineBarrier2(cmd, &dependencyInfo);

            if (mip_i < mipLevels - 1) {
                VkImageBlit2 blitRegion = {};
                blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;

                blitRegion.srcOffsets[1].x = imageSize.width;
                blitRegion.srcOffsets[1].y = imageSize.height;
                blitRegion.srcOffsets[1].z = 1;

                blitRegion.dstOffsets[1].x = halfSize.width;
                blitRegion.dstOffsets[1].y = halfSize.height;
                blitRegion.dstOffsets[1].z = 1;

                blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blitRegion.srcSubresource.baseArrayLayer = 0;
                blitRegion.srcSubresource.layerCount = 1;
                blitRegion.srcSubresource.mipLevel = mip_i;

                blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blitRegion.dstSubresource.baseArrayLayer = 0;
                blitRegion.dstSubresource.layerCount = 1;
                blitRegion.dstSubresource.mipLevel = mip_i + 1;

                VkBlitImageInfo2 blitInfo = {};
                blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
                blitInfo.dstImage = image;
                blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                blitInfo.srcImage = image;
                blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                blitInfo.filter = VK_FILTER_LINEAR;
                blitInfo.regionCount = 1;
                blitInfo.pRegions = &blitRegion;

                vkCmdBlitImage2(cmd, &blitInfo);

                imageSize = halfSize;
            }
        }

        transitionImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    VkViewport viewport(float width, float height) {
        return {
                .x = 0,
                .y = 0,
                .width = width,
                .height = height,
                .minDepth = 0.f,
                .maxDepth = 1.f,
        };
    }

    VkRect2D scissor(float width, float height) {
        return {
                .offset = {0, 0},
                .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
        };
    }

    void genCubemapMips(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize) {
        int mipLevels = int(std::floor(std::log2(std::max(imageSize.width, imageSize.height)))) + 1;

        for (size_t mip_i = 0; mip_i < mipLevels; mip_i++) {
            VkExtent2D halfSize = imageSize;
            halfSize.width /= 2;
            halfSize.height /= 2;

            VkImageMemoryBarrier2 imageBarrier = {};
            imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
            imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
            imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

            imageBarrier.subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = static_cast<uint32_t>(mip_i),
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 6,
            };
            imageBarrier.image = image;

            VkDependencyInfo dependencyInfo = {};
            dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependencyInfo.imageMemoryBarrierCount = 1;
            dependencyInfo.pImageMemoryBarriers = &imageBarrier;

            vkCmdPipelineBarrier2(cmd, &dependencyInfo);

            if (mip_i < mipLevels - 1) {
                VkImageBlit2 blitRegion = {};
                blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;

                blitRegion.srcOffsets[1].x = imageSize.width;
                blitRegion.srcOffsets[1].y = imageSize.height;
                blitRegion.srcOffsets[1].z = 1;

                blitRegion.dstOffsets[1].x = halfSize.width;
                blitRegion.dstOffsets[1].y = halfSize.height;
                blitRegion.dstOffsets[1].z = 1;

                blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blitRegion.srcSubresource.baseArrayLayer = 0;
                blitRegion.srcSubresource.layerCount = 6;
                blitRegion.srcSubresource.mipLevel = mip_i;

                blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blitRegion.dstSubresource.baseArrayLayer = 0;
                blitRegion.dstSubresource.layerCount = 6;
                blitRegion.dstSubresource.mipLevel = mip_i + 1;

                VkBlitImageInfo2 blitInfo = {};
                blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
                blitInfo.dstImage = image;
                blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                blitInfo.srcImage = image;
                blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                blitInfo.filter = VK_FILTER_LINEAR;
                blitInfo.regionCount = 1;
                blitInfo.pRegions = &blitRegion;

                vkCmdBlitImage2(cmd, &blitInfo);

                imageSize = halfSize;
            }
        }

        transitionImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    VkRenderingAttachmentInfo depthAttachment(VkImageView imageView,
                                              VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp,
                                              VkClearValue *clearValue) {
        VkRenderingAttachmentInfo depthAttachment = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = imageView,
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .loadOp = loadOp,
                .storeOp = storeOp,
                .clearValue = {
                        .depthStencil = {
                                .depth = 1.f,
                        },
                },
        };

        if (clearValue) {
            depthAttachment.clearValue = *clearValue;
        }

        return depthAttachment;
    }

    VkRenderingAttachmentInfo colorAttachment(VkImageView imageView,
                                              VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp,
                                              VkClearValue *clearValue) {
        VkRenderingAttachmentInfo colorAttachment{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = imageView,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = loadOp,
                .storeOp = storeOp,
        };

        if (clearValue) {
            colorAttachment.clearValue = *clearValue;
        }

        return colorAttachment;
    }

    VkRenderingInfo renderingInfo(uint32_t width, uint32_t height, uint32_t colorAttachmentCount,
                                  VkRenderingAttachmentInfo *colorAttachment,
                                  VkRenderingAttachmentInfo *depthAttachment,
                                  VkRenderingAttachmentInfo *stencilAttachment) {
        return {
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .pNext = nullptr,
                .flags = 0,
                .renderArea = {VkOffset2D{0, 0}, {width, height}},
                .layerCount = 1,
                .viewMask = 0,
                .colorAttachmentCount = colorAttachmentCount,
                .pColorAttachments = colorAttachment,
                .pDepthAttachment = depthAttachment,
                .pStencilAttachment = stencilAttachment,
        };
    }

    VkRenderingInfo renderingInfo(VkExtent2D extent, uint32_t colorAttachmentCount,
                                            VkRenderingAttachmentInfo *colorAttachment,
                                            VkRenderingAttachmentInfo *depthAttachment,
                                            VkRenderingAttachmentInfo *stencilAttachment) {
        return renderingInfo(extent.width, extent.height, colorAttachmentCount,
                             colorAttachment, depthAttachment, stencilAttachment);
    }
}
