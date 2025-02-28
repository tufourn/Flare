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

    VkRenderingAttachmentInfo colorAttachment(VkImageView imageView,
                                              VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                              VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                              VkClearValue *clearValue = nullptr);

    VkRenderingAttachmentInfo depthAttachment(VkImageView imageView,
                                              VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                              VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                              VkClearValue *clearValue = nullptr);

    VkRenderingInfo renderingInfo(uint32_t width, uint32_t height, uint32_t colorAttachmentCount,
                                  VkRenderingAttachmentInfo *colorAttachment = nullptr,
                                  VkRenderingAttachmentInfo *depthAttachment = nullptr,
                                  VkRenderingAttachmentInfo *stencilAttachment = nullptr);

    VkRenderingInfo renderingInfo(VkExtent2D extent, uint32_t colorAttachmentCount,
                                  VkRenderingAttachmentInfo *colorAttachment = nullptr,
                                  VkRenderingAttachmentInfo *depthAttachment = nullptr,
                                  VkRenderingAttachmentInfo *stencilAttachment = nullptr);
}

