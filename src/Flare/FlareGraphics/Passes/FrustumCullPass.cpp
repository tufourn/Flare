#include "FrustumCullPass.h"

#include "../GpuDevice.h"

namespace Flare {
    void FrustumCullPass::init(GpuDevice *gpuDevice) {
        gpu = gpuDevice;

        pipelineCI.shaderStages = {
                {"CoreShaders/FrustumCull.comp", VK_SHADER_STAGE_COMPUTE_BIT},
        };
        pipelineHandle = gpu->createPipeline(pipelineCI);

        BufferCI uniformCI = {
                .size = sizeof(FrustumCullUniforms),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .name = "frustumCullUniform",
                .bufferType = BufferType::eUniform,
        };
        frustumUniformRingBuffer.init(gpu, FRAMES_IN_FLIGHT, uniformCI);
    }

    void FrustumCullPass::addBarriers(VkCommandBuffer cmd, uint32_t computeFamily, uint32_t mainFamily) {
        Buffer *outputIndirectDrawBuffer = gpu->getBuffer(outputIndirectBufferHandle);
        Buffer *outputCountBuffer = gpu->getBuffer(outputCountBufferHandle);

        VkBufferMemoryBarrier2 barriers[] = {
                {
                        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                        .dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                        .srcQueueFamilyIndex = computeFamily,
                        .dstQueueFamilyIndex = mainFamily,
                        .buffer = outputIndirectDrawBuffer->buffer,
                        .offset = 0,
                        .size = outputIndirectDrawBuffer->size,
                },
                {
                        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                        .dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                        .srcQueueFamilyIndex = computeFamily,
                        .dstQueueFamilyIndex = mainFamily,
                        .buffer = outputCountBuffer->buffer,
                        .offset = 0,
                        .size = outputCountBuffer->size,
                },
        };

        VkDependencyInfo dep = {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .pNext = nullptr,
                .dependencyFlags = 0,
                .bufferMemoryBarrierCount = 2,
                .pBufferMemoryBarriers = barriers,
        };

        vkCmdPipelineBarrier2(cmd, &dep);
    }

    void FrustumCullPass::shutdown() {
        if (pipelineHandle.isValid()) {
            gpu->destroyPipeline(pipelineHandle);
        }
        frustumUniformRingBuffer.shutdown();
    }

    void FrustumCullPass::cull(VkCommandBuffer cmd) {
        Pipeline *pipeline = gpu->getPipeline(pipelineHandle);

        pc.uniformOffset = frustumUniformRingBuffer.buffer().index;

        vkCmdBindPipeline(cmd, pipeline->bindPoint, pipeline->pipeline);
        vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants), &pc);
        vkCmdBindDescriptorSets(cmd, pipeline->bindPoint, pipeline->pipelineLayout, 0,
                                gpu->bindlessDescriptorSets.size(), gpu->bindlessDescriptorSets.data(),
                                0, nullptr);
        vkCmdDispatch(cmd, (maxDrawCount / 256) + 1, 1, 1);
    }

    FrustumPlanes FrustumCullPass::getFrustumPlanes(glm::mat4 mat, bool normalize) {
        // if extracted from projection matrix only, planes will be in eye-space
        // if extracted from view * projection, planes will be in world space
        // if extracted from model * view * projection, planes will be in model space
        FrustumPlanes planes;

        mat = transpose(mat);

        planes.left = mat[3] + mat[0];
        planes.right = mat[3] - mat[0];
        planes.bottom = mat[3] + mat[1];
        planes.top = mat[3] - mat[1];
        planes.near = mat[3] + mat[2];
        planes.far = mat[3] - mat[2];

        if (normalize) { // hessian normal form
            planes.left /= glm::length(glm::vec3(planes.left.x, planes.left.y, planes.left.z));
            planes.right /= glm::length(glm::vec3(planes.right.x, planes.right.y, planes.right.z));
            planes.bottom /= glm::length(glm::vec3(planes.bottom.x, planes.bottom.y, planes.bottom.z));
            planes.top /= glm::length(glm::vec3(planes.top.x, planes.top.y, planes.top.z));
            planes.near /= glm::length(glm::vec3(planes.near.x, planes.near.y, planes.near.z));
            planes.far /= glm::length(glm::vec3(planes.far.x, planes.far.y, planes.far.z));
        }

        return planes;
    }

    void FrustumCullPass::updateUniforms() {
        frustumUniformRingBuffer.moveToNextBuffer();
        gpu->uploadBufferData(frustumUniformRingBuffer.buffer(), &uniforms);
    }

    void
    FrustumCullPass::setBuffers(Handle<Buffer> inputBuffer, Handle<Buffer> outputBuffer, Handle<Buffer> countBuffer,
                                Handle<Buffer> transformBuffer, Handle<Buffer> boundsBuffer) {
        maxDrawCount = gpu->getBuffer(inputBuffer)->size / sizeof(IndirectDrawData);
        outputIndirectBufferHandle = outputBuffer;
        outputCountBufferHandle = countBuffer;

        pc.data0 = inputBuffer.index;
        pc.data1 = outputBuffer.index;
        pc.data2 = countBuffer.index;
        pc.data3 = transformBuffer.index;
        pc.data4 = boundsBuffer.index;
        pc.data5 = maxDrawCount;
    }
}