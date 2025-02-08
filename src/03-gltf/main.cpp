#include "FlareApp/Application.h"
#include "FlareApp/Window.h"
#include "FlareGraphics/GpuDevice.h"
#include "FlareGraphics/ShaderCompiler.h"
#include <glm/glm.hpp>

#include "FlareGraphics/VkHelper.h"
#include "FlareGraphics/AsyncLoader.h"
#include "glm/ext/matrix_transform.hpp"
#include "FlareGraphics/GltfScene.h"

using namespace Flare;

struct TriangleApp : Application {
    struct Uniform {
        glm::mat4 mvp;
    };

    Uniform uniform = {
            .mvp = glm::rotate(glm::mat4(1.f), glm::radians(180.f), glm::vec3(0.0f, 0.0f, 1.0f))
    };

    void init(const ApplicationConfig &appConfig) override {
        WindowConfig windowConfig{};
        windowConfig
                .setWidth(appConfig.width)
                .setHeight(appConfig.height)
                .setName(appConfig.name);

        window.init(windowConfig);

        GpuDeviceCreateInfo gpuDeviceCI{
                .glfwWindow = window.glfwWindow,
        };

        gpu.init(gpuDeviceCI);
        asyncLoader.init(gpu);

        shaderCompiler.init();

        std::vector<uint32_t> shader = shaderCompiler.compile("shaders/gltf.slang");

        Flare::ReflectOutput reflection;
        std::vector<ShaderExecModel> execModels = {
                {VK_SHADER_STAGE_VERTEX_BIT,   "main"},
                {VK_SHADER_STAGE_FRAGMENT_BIT, "main"},
        };

        PipelineCI pipelineCI;
        pipelineCI.shaderStages.addBinary({execModels, shader});
        pipelineCI.rendering.colorFormats.push_back(gpu.surfaceFormat.format);

        pipelineHandle = gpu.createPipeline(pipelineCI);

        gltf.init("assets/BoxTextured.gltf", &gpu, &asyncLoader);
//        gltf.init("assets/CesiumMilkTruck.gltf", &gpu, &asyncLoader);

        BufferCI uniformBufferCI = {
                .size = sizeof(Uniform),
                .usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        };

        uniformBufferHandle = gpu.createBuffer(uniformBufferCI);

        BufferCI positionsCI = {
                .size = sizeof(glm::vec4) * gltf.positions.size(),
                .usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        };
        positionBufferHandle = gpu.createBuffer(positionsCI);
        asyncLoader.uploadRequests.emplace_back(
                UploadRequest{
                        .dstBuffer = positionBufferHandle,
                        .data = (void *) gltf.positions.data(),
                }
        );

        BufferCI indicesCI = {
                .size = sizeof(uint32_t) * gltf.indices.size(),
                .usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                              VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        };
        indexBufferHandle = gpu.createBuffer(indicesCI);
        asyncLoader.uploadRequests.emplace_back(
                UploadRequest{
                        .dstBuffer = indexBufferHandle,
                        .data = (void *) gltf.indices.data(),
                }
        );

        BufferCI uvCI = {
                .size = sizeof(glm::vec2) * gltf.uvs.size(),
                .usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        };
        uvBufferHandle = gpu.createBuffer(uvCI);
        asyncLoader.uploadRequests.emplace_back(
                UploadRequest{
                        .dstBuffer = uvBufferHandle,
                        .data = (void *) gltf.uvs.data(),
                }
        );

        BufferCI transformsCI = {
                .size = sizeof(glm::mat4) * gltf.transforms.size(),
                .usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        };
        transformBufferHandle = gpu.createBuffer(transformsCI);
        asyncLoader.uploadRequests.emplace_back(
                UploadRequest{
                        .dstBuffer = transformBufferHandle,
                        .data = (void *) gltf.transforms.data(),
                }
        );

        BufferCI meshDrawCI = {
                .size = sizeof(MeshDraw) * gltf.meshDraws.size(),
                .usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        };
        meshDrawHandle = gpu.createBuffer(meshDrawCI);
//        asyncLoader.uploadRequests.emplace_back(
//                UploadRequest{
//                        .dstBuffer = meshDrawHandle,
//                        .data = (void *) gltf.meshDraws.data(),
//                }
//        );

        Pipeline *pipeline = gpu.pipelines.get(pipelineHandle);

        DescriptorSetCI descCI = {
                .layout = pipeline->descriptorSetLayoutHandles[0],
        };
        descCI
                .addBuffer(positionBufferHandle, 0)
                .addBuffer(uvBufferHandle, 1)
                .addBuffer(transformBufferHandle, 2)
                .addBuffer(uniformBufferHandle, 3);

        descriptorSetHandle = gpu.createDescriptorSet(descCI);
    }

    void loop() override {
        while (!window.shouldClose()) {
            window.pollEvents();

            uniform.mvp = glm::rotate(glm::mat4(1.f), static_cast<float>(gpu.absoluteFrame / 10000.f),
                                      glm::vec3(0.0f, 1.0f, 0.0f));
            asyncLoader.uploadRequests.emplace_back(
                    UploadRequest{
                            .dstBuffer = uniformBufferHandle,
                            .data = (void *) &uniform,
                    }
            );

            while (!asyncLoader.uploadRequests.empty()) {
                asyncLoader.update();
            }

            if (!window.isMinimized()) {
                gpu.newFrame();

                VkCommandBuffer cmd = gpu.getCommandBuffer();

                asyncLoader.signalTextures(cmd);
                asyncLoader.signalBuffers(cmd);

                VkHelper::transitionImage(cmd, gpu.swapchainImages[gpu.swapchainImageIndex],
                                          VK_IMAGE_LAYOUT_UNDEFINED,
                                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

                VkRenderingAttachmentInfo colorAttachment = {
                        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                        .pNext = nullptr,
                        .imageView = gpu.swapchainImageViews[gpu.swapchainImageIndex],
                        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                };

                VkRenderingInfo renderingInfo = {
                        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                        .pNext = nullptr,
                        .flags = 0,
                        .renderArea = {VkOffset2D{0, 0}, gpu.swapchainExtent},
                        .layerCount = 1,
                        .viewMask = 0,
                        .colorAttachmentCount = 1,
                        .pColorAttachments = &colorAttachment,
                        .pDepthAttachment = nullptr,
                        .pStencilAttachment = nullptr,
                };

                Pipeline *pipeline = gpu.pipelines.get(pipelineHandle);

                vkCmdBeginRendering(cmd, &renderingInfo);
                vkCmdBindPipeline(cmd, pipeline->bindPoint, pipeline->pipeline);

                vkCmdBindIndexBuffer(cmd, gpu.buffers.get(indexBufferHandle)->buffer, 0, VK_INDEX_TYPE_UINT32);

                vkCmdBindDescriptorSets(cmd, pipeline->bindPoint, pipeline->pipelineLayout, BINDLESS_SET, 1,
                                        &gpu.descriptorSets.get(gpu.bindlessDescriptorSetHandle)->descriptorSet, 0,
                                        nullptr);

                vkCmdBindDescriptorSets(cmd, pipeline->bindPoint, pipeline->pipelineLayout, 1, 1,
                                        &gpu.descriptorSets.get(descriptorSetHandle)->descriptorSet, 0,
                                        nullptr);

                VkViewport viewport = {
                        .x = 0.f,
                        .y = 0.f,
                        .width = static_cast<float>(gpu.swapchainExtent.width),
                        .height = static_cast<float>(gpu.swapchainExtent.height),
                        .minDepth = 0.f,
                        .maxDepth = 1.f,
                };
                vkCmdSetViewport(cmd, 0, 1, &viewport);

                VkRect2D scissor = {
                        .offset = {0, 0},
                        .extent = gpu.swapchainExtent,
                };
                vkCmdSetScissor(cmd, 0, 1, &scissor);

                for (const auto &md: gltf.meshDraws) {
                    vkCmdDrawIndexed(cmd, md.indexCount, 1, md.indexOffset, md.positionOffset, 0);
                }

                vkCmdEndRendering(cmd);

                VkHelper::transitionImage(cmd, gpu.swapchainImages[gpu.swapchainImageIndex],
                                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

                vkEndCommandBuffer(cmd);

                gpu.present();
            }
        }
    }

    void shutdown() override {
        vkDeviceWaitIdle(gpu.device);

        gltf.shutdown();

        gpu.destroyPipeline(pipelineHandle);
        gpu.destroyBuffer(positionBufferHandle);
        gpu.destroyBuffer(indexBufferHandle);
        gpu.destroyBuffer(transformBufferHandle);
        gpu.destroyBuffer(uvBufferHandle);
        gpu.destroyBuffer(meshDrawHandle);
        gpu.destroyBuffer(uniformBufferHandle);

        asyncLoader.shutdown();
        gpu.shutdown();
        window.shutdown();
    }

    Flare::GpuDevice gpu;
    Flare::Window window;
    Flare::ShaderCompiler shaderCompiler;
    Flare::AsyncLoader asyncLoader;

    Handle<Pipeline> pipelineHandle;

    Handle<Buffer> uniformBufferHandle;
    Handle<Buffer> positionBufferHandle;
    Handle<Buffer> indexBufferHandle;
    Handle<Buffer> transformBufferHandle;
    Handle<Buffer> uvBufferHandle;
    Handle<Buffer> meshDrawHandle;

    Handle<DescriptorSet> descriptorSetHandle;

    GltfScene gltf;
};

int main() {
    Flare::ApplicationConfig appConfig{};
    appConfig
            .setWidth(800)
            .setHeight(600)
            .setName("Triangle");

    TriangleApp app;
    app.run(appConfig);
}