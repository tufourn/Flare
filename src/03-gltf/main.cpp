#include "FlareApp/Application.h"
#include "FlareApp/Window.h"
#include "FlareApp/Camera.h"
#include "FlareGraphics/GpuDevice.h"
#include "FlareGraphics/Passes/ShadowPass.h"

#include <glm/glm.hpp>

#include "FlareGraphics/VkHelper.h"
#include "FlareGraphics/AsyncLoader.h"
#include "FlareGraphics/GltfScene.h"
#include "FlareGraphics/RingBuffer.h"
#include "FlareGraphics/FlareImgui.h"
#include "imgui.h"

using namespace Flare;

struct TriangleApp : Application {
    struct Globals {
        glm::mat4 mvp;
        glm::mat4 lightSpaceMatrix;

        uint32_t positionBufferIndex;
        uint32_t normalBufferIndex;
        uint32_t uvBufferIndex;
        uint32_t transformBufferIndex;

        uint32_t textureBufferIndex;
        uint32_t materialBufferIndex;
        uint32_t drawDataBufferIndex;
        uint32_t tangentBufferIndex;

        Light light;

        uint32_t shadowDepthTextureIndex;
        uint32_t shadowSamplerIndex;
        float pad[2];
    };

    Globals globals;

    struct ShadowUniform {
        glm::mat4 lightSpaceMatrix;

        uint32_t drawDataBufferIndex;
        uint32_t positionBufferIndex;
        uint32_t transformBufferIndex;
        float pad;
    };

    ShadowUniform shadowUniforms;

    struct GpuDrawData {
        uint32_t transformOffset;
        uint32_t materialOffset;
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

        pipelineCI.shaderStages.emplace_back(ShaderStage{"shaders/gltf.vert", VK_SHADER_STAGE_VERTEX_BIT});
        pipelineCI.shaderStages.emplace_back(ShaderStage{"shaders/gltf.frag", VK_SHADER_STAGE_FRAGMENT_BIT});
        pipelineCI.rendering.colorFormats.push_back(gpu.surfaceFormat.format);
        pipelineCI.rendering.depthFormat = VK_FORMAT_D32_SFLOAT;
        pipelineCI.rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
        pipelineCI.depthStencil = {
                .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
                .depthTestEnable = true,
                .depthWriteEnable = true,
        };

        pipelineHandle = gpu.createPipeline(pipelineCI);

//        gltf.init("assets/Triangle.gltf", &gpu, &asyncLoader);
//        gltf.init("assets/BoxTextured.gltf", &gpu, &asyncLoader);
//        gltf.init("assets/CesiumMilkTruck.gltf", &gpu, &asyncLoader);
//        gltf.init("assets/structure.glb", &gpu, &asyncLoader);
        gltf.init("assets/Sponza/glTF/Sponza.gltf", &gpu, &asyncLoader);

        BufferCI globalsBufferCI = {
                .size = sizeof(Globals),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .name = "globals",
        };
        globalsRingBuffer.init(&gpu, FRAMES_IN_FLIGHT, globalsBufferCI, RingBuffer::Type::eUniform);

        BufferCI shadowUniformBufferCI = {
                .size = sizeof(ShadowUniform),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .name = "shadowUniforms",
        };
        shadowUniformBuffer.init(&gpu, FRAMES_IN_FLIGHT, shadowUniformBufferCI, RingBuffer::Type::eUniform);

        BufferCI positionsCI = {
                .size = sizeof(glm::vec4) * gltf.positions.size(),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .name = "positions",
        };
        positionBufferHandle = gpu.createBuffer(positionsCI);
        asyncLoader.uploadRequests.emplace_back(
                UploadRequest{
                        .dstBuffer = gpu.getBuffer(positionBufferHandle),
                        .data = (void *) gltf.positions.data(),
                }
        );

        BufferCI indicesCI = {
                .size = sizeof(uint32_t) * gltf.indices.size(),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                .name = "indices",
        };
        indexBufferHandle = gpu.createBuffer(indicesCI);
        asyncLoader.uploadRequests.emplace_back(
                UploadRequest{
                        .dstBuffer = gpu.getBuffer(indexBufferHandle),
                        .data = (void *) gltf.indices.data(),
                }
        );

        BufferCI textureIndicesCI = {
                .size = sizeof(GltfTexture) * gltf.gltfTextures.size(),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .name = "textures",
        };
        textureIndicesHandle = gpu.createBuffer(textureIndicesCI);
        asyncLoader.uploadRequests.emplace_back(
                UploadRequest{
                        .dstBuffer = gpu.getBuffer(textureIndicesHandle),
                        .data = (void *) gltf.gltfTextures.data(),
                }
        );

        BufferCI uvCI = {
                .size = sizeof(glm::vec2) * gltf.uvs.size(),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .name = "uv",
        };
        uvBufferHandle = gpu.createBuffer(uvCI);
        asyncLoader.uploadRequests.emplace_back(
                UploadRequest{
                        .dstBuffer = gpu.getBuffer(uvBufferHandle),
                        .data = (void *) gltf.uvs.data(),
                }
        );

        BufferCI tangentCI = {
                .size = sizeof(glm::vec4) * gltf.tangents.size(),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .name = "tangents",
        };
        tangentBufferHandle = gpu.createBuffer(tangentCI);
        asyncLoader.uploadRequests.emplace_back(
                UploadRequest{
                        .dstBuffer = gpu.getBuffer(tangentBufferHandle),
                        .data = (void *) gltf.tangents.data(),
                }
        );

        BufferCI transformsCI = {
                .size = sizeof(glm::mat4) * gltf.transforms.size(),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .name = "transforms",
        };
        transformBufferHandle = gpu.createBuffer(transformsCI);
        asyncLoader.uploadRequests.emplace_back(
                UploadRequest{
                        .dstBuffer = gpu.getBuffer(transformBufferHandle),
                        .data = (void *) gltf.transforms.data(),
                }
        );

        BufferCI materialCI = {
                .size = sizeof(Material) * gltf.materials.size(),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .name = "materials",
        };
        materialBufferHandle = gpu.createBuffer(materialCI);
        asyncLoader.uploadRequests.emplace_back(
                UploadRequest{
                        .dstBuffer = gpu.getBuffer(materialBufferHandle),
                        .data = (void *) gltf.materials.data(),
                }
        );

        BufferCI normalCI = {
                .size = sizeof(glm::vec4) * gltf.normals.size(),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .name = "normals",
        };
        normalBufferHandle = gpu.createBuffer(normalCI);
        asyncLoader.uploadRequests.emplace_back(
                UploadRequest{
                        .dstBuffer = gpu.getBuffer(normalBufferHandle),
                        .data = (void *) gltf.normals.data(),
                }
        );

        BufferCI meshDrawCI = {
                .size = sizeof(GpuDrawData) * gltf.meshDraws.size(),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .name = "meshdraws"
        };

        gpuMeshDraws.reserve(gltf.meshDraws.size());
        for (auto &meshDraw: gltf.meshDraws) {
            gpuMeshDraws.emplace_back(GpuDrawData{
                    .transformOffset = meshDraw.transformOffset,
                    .materialOffset = meshDraw.materialOffset,
            });
        }

        drawDataBufferHandle = gpu.createBuffer(meshDrawCI);
        asyncLoader.uploadRequests.emplace_back(
                UploadRequest{
                        .dstBuffer = gpu.getBuffer(drawDataBufferHandle),
                        .data = (void *) gpuMeshDraws.data(),
                }
        );

        indirectDraws.resize(gltf.meshDraws.size());
        BufferCI indirectDrawCI = {
                .size = sizeof(IndirectDrawCommand) * gltf.meshDraws.size(),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                .mapped = true,
                .name = "indirectDraws",
        };
        indirectDrawBufferHandle = gpu.createBuffer(indirectDrawCI);

        shadowPass.init(&gpu);

        globals.positionBufferIndex = positionBufferHandle.index;
        globals.normalBufferIndex = normalBufferHandle.index;
        globals.uvBufferIndex = uvBufferHandle.index;
        globals.transformBufferIndex = transformBufferHandle.index;
        globals.textureBufferIndex = textureIndicesHandle.index;
        globals.materialBufferIndex = materialBufferHandle.index;
        globals.drawDataBufferIndex = drawDataBufferHandle.index;
        globals.tangentBufferIndex = tangentBufferHandle.index;
        globals.shadowDepthTextureIndex = shadowPass.depthTextureHandle.index;
        globals.shadowSamplerIndex = shadowPass.samplerHandle.index;

        globals.light = {
                .position = {1.f, 20.f, 2.f},
                .radius = 1.f,
                .color = {1.f, 1.f, 1.f},
                .intensity = 1.f,
        };

        shadowUniforms.drawDataBufferIndex = drawDataBufferHandle.index;
        shadowUniforms.positionBufferIndex = positionBufferHandle.index;
        shadowUniforms.transformBufferIndex = transformBufferHandle.index;

        glfwSetWindowUserPointer(window.glfwWindow, &camera);
        glfwSetCursorPosCallback(window.glfwWindow, Camera::mouseCallback);
        glfwSetKeyCallback(window.glfwWindow, Camera::keyCallback);
        glfwSetMouseButtonCallback(window.glfwWindow, Camera::mouseButtonCallback);

        imgui.init(&gpu);
    }

    void loop() override {
        while (!window.shouldClose()) {
            window.newFrame();

            if (window.shouldResize) {
                window.shouldResize = false;

                gpu.resizeSwapchain();
            }

            if (!window.isMinimized()) {
                camera.update();

                float nearPlane = 1.f;
                float farPlane = 100.f;
                glm::mat4 lightView = glm::lookAt(globals.light.position, glm::vec3(0.f, 0.f, 0.f),
                                                  glm::vec3(0.f, 1.f, 0.f));
                glm::mat4 lightProjection = glm::ortho(-10.f, 10.f, -10.f, 10.f, nearPlane, farPlane);
                lightProjection[1][1] *= -1;
                shadowUniforms.lightSpaceMatrix = lightProjection * lightView;
                asyncLoader.uploadRequests.emplace_back(
                        UploadRequest{
                                .dstBuffer = gpu.getUniform(shadowUniformBuffer.buffer()),
                                .data = (void *) &shadowUniforms,
                        }
                );

                glm::mat4 view = camera.getViewMatrix();
                glm::mat4 projection = glm::perspectiveZO(glm::radians(45.f),
                                                          static_cast<float>(window.width) / window.height, 0.1f,
                                                          100.f);
                projection[1][1] *= -1;
                globals.mvp = projection * view;
                globals.lightSpaceMatrix = shadowUniforms.lightSpaceMatrix;

                asyncLoader.uploadRequests.emplace_back(
                        UploadRequest{
                                .dstBuffer = gpu.getUniform(globalsRingBuffer.buffer()),
                                .data = (void *) &globals,
                        }
                );

                while (!asyncLoader.uploadRequests.empty()) { //todo: proper async transfer
                    asyncLoader.update();
                }

                PushConstants pc = {
                        .uniformOffset = globalsRingBuffer.buffer().index,
                };

                for (size_t i = 0; i < gltf.meshDraws.size(); i++) {
                    const MeshDraw &md = gltf.meshDraws[i];

                    indirectDraws[i].cmd = {
                            .indexCount = md.indexCount,
                            .instanceCount = 1,
                            .firstIndex = md.indexOffset,
                            .vertexOffset = static_cast<int32_t>(md.vertexOffset),
                            .firstInstance = 0,
                    };
                }

                memcpy(gpu.getBuffer(indirectDrawBufferHandle)->allocationInfo.pMappedData, indirectDraws.data(),
                       sizeof(IndirectDrawCommand) * indirectDraws.size());

                gpu.newFrame();
                imgui.newFrame();

                VkCommandBuffer cmd = gpu.getCommandBuffer();

                asyncLoader.signalTextures(cmd);
                asyncLoader.signalBuffers(cmd);

                shadowPass.render(cmd,
                                  shadowUniformBuffer.buffer().index,
                                  indexBufferHandle,
                                  indirectDrawBufferHandle, indirectDraws.size());

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

                VkRenderingAttachmentInfo depthAttachment = {
                        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                        .pNext = nullptr,
                        .imageView = gpu.textures.get(gpu.depthTextures[gpu.swapchainImageIndex])->imageView,
                        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                        .clearValue = {
                                .depthStencil = {
                                        .depth = 1.f,
                                },
                        },
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
                        .pDepthAttachment = &depthAttachment,
                        .pStencilAttachment = nullptr,
                };

                Pipeline *pipeline = gpu.pipelines.get(pipelineHandle);

                vkCmdBeginRendering(cmd, &renderingInfo);
                vkCmdBindPipeline(cmd, pipeline->bindPoint, pipeline->pipeline);

                vkCmdBindIndexBuffer(cmd, gpu.buffers.get(indexBufferHandle)->buffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdBindDescriptorSets(cmd, pipeline->bindPoint, pipeline->pipelineLayout, 0,
                                        gpu.bindlessDescriptorSets.size(), gpu.bindlessDescriptorSets.data(),
                                        0, nullptr);

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

                vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants),
                                   &pc);

                vkCmdDrawIndexedIndirect(cmd, gpu.getBuffer(indirectDrawBufferHandle)->buffer, 0, gltf.meshDraws.size(),
                                         sizeof(IndirectDrawCommand));

                vkCmdEndRendering(cmd);

                ImGui::Begin("Light");
                ImGui::SliderFloat3("Light position", reinterpret_cast<float *>(&globals.light.position), -50.f, 50.f);
                ImGui::SliderFloat("Light intensity", reinterpret_cast<float *>(&globals.light.intensity), 0.f, 100.f);
                if (ImGui::Button("Reload pipeline")) {
                    shouldReloadPipeline = true;
                }
                ImGui::End();

                ImGui::Render();
                imgui.draw(cmd, gpu.swapchainImageViews[gpu.swapchainImageIndex]);

                VkHelper::transitionImage(cmd, gpu.swapchainImages[gpu.swapchainImageIndex],
                                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

                vkEndCommandBuffer(cmd);

                globalsRingBuffer.moveToNextBuffer();
                shadowUniformBuffer.moveToNextBuffer();

                gpu.present();

                if (shouldReloadPipeline) {
                    gpu.recreatePipeline(pipelineHandle, pipelineCI);
                    shouldReloadPipeline = false;
                }
            }
        }
    }

    void shutdown() override {
        vkDeviceWaitIdle(gpu.device);

        shadowPass.shutdown();

        imgui.shutdown();

        gltf.shutdown();

        globalsRingBuffer.shutdown();
        shadowUniformBuffer.shutdown();

        gpu.destroyPipeline(pipelineHandle);
        gpu.destroyBuffer(positionBufferHandle);
        gpu.destroyBuffer(indexBufferHandle);
        gpu.destroyBuffer(transformBufferHandle);
        gpu.destroyBuffer(uvBufferHandle);
        gpu.destroyBuffer(drawDataBufferHandle);
        gpu.destroyBuffer(textureIndicesHandle);
        gpu.destroyBuffer(materialBufferHandle);
        gpu.destroyBuffer(normalBufferHandle);
        gpu.destroyBuffer(indirectDrawBufferHandle);
        gpu.destroyBuffer(tangentBufferHandle);

        asyncLoader.shutdown();
        gpu.shutdown();
        window.shutdown();
    }

    Flare::GpuDevice gpu;
    Flare::Window window;
    Flare::AsyncLoader asyncLoader;

    bool shouldReloadPipeline = false;

    PipelineCI pipelineCI;
    Handle<Pipeline> pipelineHandle;

    RingBuffer globalsRingBuffer;
    RingBuffer shadowUniformBuffer;

    Handle<Buffer> positionBufferHandle;
    Handle<Buffer> indexBufferHandle;
    Handle<Buffer> transformBufferHandle;
    Handle<Buffer> uvBufferHandle;
    Handle<Buffer> tangentBufferHandle;
    Handle<Buffer> drawDataBufferHandle;
    Handle<Buffer> textureIndicesHandle;
    Handle<Buffer> materialBufferHandle;
    Handle<Buffer> normalBufferHandle;
    std::vector<IndirectDrawCommand> indirectDraws;
    Handle<Buffer> indirectDrawBufferHandle;
    std::vector<GpuDrawData> gpuMeshDraws;

    Camera camera;

    GltfScene gltf;

    FlareImgui imgui;

    ShadowPass shadowPass;
};

int main() {
    Flare::ApplicationConfig appConfig{};
    appConfig
            .setWidth(1280)
            .setHeight(720)
            .setName("Triangle");

    TriangleApp app;
    app.run(appConfig);
}