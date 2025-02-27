#include "AsyncLoader.h"

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>
#include "VkHelper.h"

namespace Flare {
    static constexpr size_t STAGING_BUFFER_SIZE_MB = 64;

    void AsyncLoader::init(GpuDevice &gpuDevice) {
        gpu = &gpuDevice;

        for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
            VkCommandPoolCreateInfo commandPoolCI = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                    .queueFamilyIndex = gpu->transferFamily,
            };
            vkCreateCommandPool(gpu->device, &commandPoolCI, nullptr, &commandPools[i]);

            VkCommandBufferAllocateInfo cmd = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                    .pNext = nullptr,
                    .commandPool = commandPools[i],
                    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                    .commandBufferCount = 1,
            };
            vkAllocateCommandBuffers(gpu->device, &cmd, &commandBuffers[i]);
        }

        VkFenceCreateInfo fenceCI = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .pNext = nullptr,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        vkCreateFence(gpu->device, &fenceCI, nullptr, &transferFence);

        BufferCI stagingBufferCI = {
                .size = STAGING_BUFFER_SIZE_MB * 1024 * 1024,
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .mapped = true,
        };

        stagingBufferHandle = gpu->createBuffer(stagingBufferCI);
    }

    void AsyncLoader::shutdown() {
        vkDeviceWaitIdle(gpu->device);

        vkDestroyFence(gpu->device, transferFence, nullptr);

        gpu->destroyBuffer(stagingBufferHandle);
        for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
            vkDestroyCommandPool(gpu->device, commandPools[i], nullptr);
        }
    }

    void AsyncLoader::update() {
        if (!fileRequests.empty()) {
            FileRequest fileRequest = fileRequests.back();
            fileRequests.pop_back();

            spdlog::info("loading {}", fileRequest.path.string());

            int width, height, component;
            unsigned char *stbData = stbi_load(fileRequest.path.string().c_str(),
                                               &width, &height, &component, STBI_rgb_alpha);
            uploadRequests.emplace_back(
                    UploadRequest{
                            .texture = fileRequest.texture,
                            .data = stbData,
                    }
            );
        }

        if (!uploadRequests.empty()) {
            if (vkGetFenceStatus(gpu->device, transferFence) != VK_SUCCESS) {
                return;
            }
            vkResetFences(gpu->device, 1, &transferFence);

            UploadRequest request = uploadRequests.back();
            uploadRequests.pop_back();

            VkCommandBuffer cmd = commandBuffers[gpu->currentFrame];

            VkCommandBufferBeginInfo beginInfo = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    .pNext = nullptr,
                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                    .pInheritanceInfo = nullptr,
            };

            vkBeginCommandBuffer(cmd, &beginInfo);

            if (request.texture && request.data) {
                constexpr size_t channelCount = 4;
                const size_t imageSize = request.texture->width * request.texture->height * channelCount;

                const size_t alignedImageSize = VkHelper::memoryAlign(imageSize, 4);
                size_t offset = std::atomic_fetch_add(&stagingBufferOffset, alignedImageSize);

                Buffer *stagingBuffer = gpu->buffers.get(stagingBufferHandle);
                memcpy(static_cast<std::byte *>(stagingBuffer->allocationInfo.pMappedData) + offset,
                       request.data, imageSize);

                VkImageMemoryBarrier2 preCopyBarrier = {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                        .pNext = nullptr,
                        .srcStageMask = 0,
                        .srcAccessMask = 0,
                        .dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
                        .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .image = request.texture->image,
                        .subresourceRange = {
                                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                .baseMipLevel = 0,
                                .levelCount = VK_REMAINING_MIP_LEVELS,
                                .baseArrayLayer = 0,
                                .layerCount = VK_REMAINING_ARRAY_LAYERS,
                        }
                };

                VkDependencyInfo preCopyDep = {
                        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                        .pNext = nullptr,
                        .dependencyFlags = 0,
                        .imageMemoryBarrierCount = 1,
                        .pImageMemoryBarriers = &preCopyBarrier,
                };

                vkCmdPipelineBarrier2(cmd, &preCopyDep);

                VkBufferImageCopy2 region = {
                        .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                        .pNext = nullptr,
                        .bufferOffset = offset,
                        .bufferRowLength = 0,
                        .bufferImageHeight = 0,
                        .imageSubresource = {
                                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                .mipLevel = 0,
                                .baseArrayLayer = 0,
                                .layerCount = 1,
                        },
                        .imageOffset = {0, 0, 0},
                        .imageExtent = {
                                .width = request.texture->width,
                                .height = request.texture->height,
                                .depth = request.texture->depth,
                        },
                };

                VkCopyBufferToImageInfo2 copyInfo = {
                        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
                        .pNext = nullptr,
                        .srcBuffer = stagingBuffer->buffer,
                        .dstImage = request.texture->image,
                        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        .regionCount = 1,
                        .pRegions = &region,
                };

                vkCmdCopyBufferToImage2(cmd, &copyInfo);

                VkImageMemoryBarrier2 postCopyBarrier = {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                        .pNext = nullptr,
                        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        .dstAccessMask = 0,
                        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                        .srcQueueFamilyIndex = gpu->transferFamily,
                        .dstQueueFamilyIndex = gpu->mainFamily,
                        .image = request.texture->image,
                        .subresourceRange = {
                                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                .baseMipLevel = 0,
                                .levelCount = VK_REMAINING_MIP_LEVELS,
                                .baseArrayLayer = 0,
                                .layerCount = VK_REMAINING_ARRAY_LAYERS,
                        }
                };

                VkDependencyInfo postCopyDep = {
                        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                        .pNext = nullptr,
                        .dependencyFlags = 0,
                        .imageMemoryBarrierCount = 1,
                        .pImageMemoryBarriers = &postCopyBarrier,
                };

                vkCmdPipelineBarrier2(cmd, &postCopyDep);

                free(request.data);
            } else if (request.srcBuffer && request.dstBuffer) {
                VkBufferCopy region = {
                        .srcOffset = 0,
                        .dstOffset = 0,
                        .size = request.srcBuffer->size,
                };

                vkCmdCopyBuffer(cmd, request.srcBuffer->buffer, request.dstBuffer->buffer, 1, &region);
            } else if (request.dstBuffer) {
                const size_t alignedBufferSize = VkHelper::memoryAlign(request.dstBuffer->size, 64);
                size_t offset = std::atomic_fetch_add(&stagingBufferOffset, alignedBufferSize);

                Buffer *stagingBuffer = gpu->buffers.get(stagingBufferHandle);
                memcpy(static_cast<std::byte *>(stagingBuffer->allocationInfo.pMappedData) + offset,
                       request.data, request.dstBuffer->size);

                VkBufferCopy2 region = {
                        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                        .pNext = nullptr,
                        .srcOffset = offset,
                        .dstOffset = 0,
                        .size = request.dstBuffer->size,
                };

                VkCopyBufferInfo2 copyInfo = {
                        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                        .pNext = nullptr,
                        .srcBuffer = stagingBuffer->buffer,
                        .dstBuffer = request.dstBuffer->buffer,
                        .regionCount = 1,
                        .pRegions = &region,
                };

                vkCmdCopyBuffer2(cmd, &copyInfo);

                VkBufferMemoryBarrier2 barrier = {
                        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                        .pNext = nullptr,
                        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        .dstAccessMask = 0,
                        .srcQueueFamilyIndex = gpu->transferFamily,
                        .dstQueueFamilyIndex = gpu->mainFamily,
                        .buffer = request.dstBuffer->buffer,
                        .offset = 0,
                        .size = request.dstBuffer->size,
                };

                VkDependencyInfo depInfo = {
                        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                        .pNext = nullptr,
                        .dependencyFlags = 0,
                        .bufferMemoryBarrierCount = 1,
                        .pBufferMemoryBarriers = &barrier,
                };

                vkCmdPipelineBarrier2(cmd, &depInfo);
            }

            vkEndCommandBuffer(cmd);

            VkCommandBufferSubmitInfo cmdSubmitInfo = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                    .pNext = nullptr,
                    .commandBuffer = cmd,
            };

            VkSubmitInfo2 submitInfo = {
                    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                    .pNext = nullptr,
                    .flags = 0,
                    .commandBufferInfoCount = 1,
                    .pCommandBufferInfos = &cmdSubmitInfo,
            };

            vkQueueSubmit2(gpu->transferQueue, 1, &submitInfo, transferFence);

            if (request.texture && request.texture->mipLevel > 1) {
                VkHelper::genMips(gpu->getCommandBuffer(), request.texture->image,
                                  {request.texture->width, request.texture->height});
            }

            if (request.texture) {
                std::lock_guard<std::mutex> lock(textureMutex);
                pendingTextures.push_back(request.texture);
            } else if (request.dstBuffer) {
                std::lock_guard<std::mutex> lock(bufferMutex);
                pendingBuffers.push_back(request.dstBuffer);
            }
        }

        stagingBufferOffset = 0;
    }

    void AsyncLoader::signalTextures(VkCommandBuffer cmd) {
        std::lock_guard<std::mutex> lock(textureMutex);

        if (pendingTextures.empty()) {
            return;
        }

        for (const auto &texture: pendingTextures) {
            VkImageMemoryBarrier2 barrier = {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .pNext = nullptr,
                    .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .dstAccessMask = 0,
                    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .srcQueueFamilyIndex = gpu->transferFamily,
                    .dstQueueFamilyIndex = gpu->mainFamily,
                    .image = texture->image,
                    .subresourceRange = {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .baseMipLevel = 0,
                            .levelCount = VK_REMAINING_MIP_LEVELS,
                            .baseArrayLayer = 0,
                            .layerCount = VK_REMAINING_ARRAY_LAYERS,
                    }
            };

            VkDependencyInfo postCopyDep = {
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .pNext = nullptr,
                    .dependencyFlags = 0,
                    .imageMemoryBarrierCount = 1,
                    .pImageMemoryBarriers = &barrier,
            };

            vkCmdPipelineBarrier2(cmd, &postCopyDep);
        }

        pendingTextures.clear();
    }

    void AsyncLoader::signalBuffers(VkCommandBuffer cmd) {
        std::lock_guard<std::mutex> lock(bufferMutex);

        if (pendingBuffers.empty()) {
            return;
        }

        for (const auto &buffer: pendingBuffers) {
            VkBufferMemoryBarrier2 barrier = {
                    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                    .pNext = nullptr,
                    .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .srcAccessMask = 0,
                    .dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
                    .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
                    .srcQueueFamilyIndex = gpu->transferFamily,
                    .dstQueueFamilyIndex = gpu->mainFamily,
                    .buffer = buffer->buffer,
                    .offset = 0,
                    .size = buffer->size
            };

            VkDependencyInfo dep = {
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .pNext = nullptr,
                    .dependencyFlags = 0,
                    .bufferMemoryBarrierCount = 1,
                    .pBufferMemoryBarriers = &barrier,
            };

            vkCmdPipelineBarrier2(cmd, &dep);
        }

        pendingBuffers.clear();
    }
}