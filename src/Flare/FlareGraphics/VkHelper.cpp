#include "VkHelper.h"

namespace VkHelper {
    void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout initialLayout, VkImageLayout finalLayout) {
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
                .subresourceRange = {
                        .aspectMask = static_cast<VkImageAspectFlags>(finalLayout ==
                                                                      VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL ?
                                                                      VK_IMAGE_ASPECT_DEPTH_BIT :
                                                                      VK_IMAGE_ASPECT_COLOR_BIT),
                        .baseMipLevel = 0,
                        .levelCount = VK_REMAINING_MIP_LEVELS,
                        .baseArrayLayer = 0,
                        .layerCount = VK_REMAINING_ARRAY_LAYERS,
                },
        };

        VkDependencyInfo dependencyInfo = {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .pNext = nullptr,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &imageBarrier,
        };

        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }
}
