#include "FlareApp/Application.h"
#include "FlareApp/Window.h"
#include "FlareApp/Camera.h"
#include "FlareGraphics/GpuDevice.h"
#include "FlareGraphics/Passes/ShadowPass.h"
#include "FlareGraphics/Passes/FrustumCullPass.h"

#include <glm/glm.hpp>

#include "FlareGraphics/VkHelper.h"
#include "FlareGraphics/AsyncLoader.h"
#include "FlareGraphics/GltfScene.h"
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
        uint32_t indirectDrawDataBufferIndex;
        uint32_t tangentBufferIndex;

        Light light;

        uint32_t shadowDepthTextureIndex;
        uint32_t shadowSamplerIndex;
        float pad[2];
    };

    Globals globals;

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

        indirectDrawDatas.resize(gltf.meshDraws.size());
        BufferCI indirectDrawCI = {
                .size = sizeof(IndirectDrawData) * gltf.meshDraws.size(),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                .mapped = true,
                .name = "indirectDraws",
        };
        indirectDrawDataBufferHandle = gpu.createBuffer(indirectDrawCI);

        BufferCI culledIndirectDrawCI = {
                .size = sizeof(IndirectDrawData) * indirectDrawDatas.size(),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                .name = "culledIndirectDraws",
        };
        culledIndirectDrawDataBufferHandle = gpu.createBuffer(culledIndirectDrawCI);

        BufferCI indirectCountCI = {
                .size = sizeof(uint32_t),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                .readback = true,
                .name = "indirectCount",
        };
        countBufferHandle = gpu.createBuffer(indirectCountCI);

        bounds.resize(gltf.meshDraws.size());
        BufferCI boundCI = {
                .size = sizeof(Bounds) * gltf.meshDraws.size(),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                .mapped = true,
                .name = "bounds",
        };
        boundsBufferHandle = gpu.createBuffer(boundCI);

        shadowPass.init(&gpu);
        frustumCullPass.init(&gpu);

        globals.positionBufferIndex = positionBufferHandle.index;
        globals.normalBufferIndex = normalBufferHandle.index;
        globals.uvBufferIndex = uvBufferHandle.index;
        globals.transformBufferIndex = transformBufferHandle.index;

        globals.textureBufferIndex = textureIndicesHandle.index;
        globals.materialBufferIndex = materialBufferHandle.index;
        globals.tangentBufferIndex = tangentBufferHandle.index;

        globals.shadowDepthTextureIndex = shadowPass.depthTextureHandle.index;
        globals.shadowSamplerIndex = shadowPass.samplerHandle.index;

        globals.light = {
                .position = {-4.f, 12.f, 2.f},
                .radius = 1.f,
                .color = {1.f, 1.f, 1.f},
                .intensity = 1.f,
        };

        // shadow uniforms
        shadowPass.uniforms.indirectDrawDataBufferIndex = indirectDrawDataBufferHandle.index;
        shadowPass.uniforms.positionBufferIndex = positionBufferHandle.index;
        shadowPass.uniforms.transformBufferIndex = transformBufferHandle.index;

        // frustum cull uniforms
        frustumCullPass.uniforms.transformBufferIndex = transformBufferHandle.index;
        frustumCullPass.uniforms.inputIndirectDrawDataBufferIndex = indirectDrawDataBufferHandle.index;
        frustumCullPass.uniforms.outputIndirectDrawDataBufferIndex = culledIndirectDrawDataBufferHandle.index;
        frustumCullPass.uniforms.boundsBufferIndex = boundsBufferHandle.index;
        frustumCullPass.uniforms.countBufferIndex = countBufferHandle.index;

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
                camera.setAspectRatio((float) window.width / window.height);

                float nearPlane = 1.f;
                float farPlane = 100.f;
                glm::mat4 lightView = glm::lookAt(globals.light.position, glm::vec3(0.f, 0.f, 0.f),
                                                  glm::vec3(0.f, 1.f, 0.f));
                glm::mat4 lightProjection = glm::ortho(-20.f, 20.f, -20.f, 20.f, nearPlane, farPlane);
                lightProjection[1][1] *= -1;
                glm::mat4 lightSpaceMatrix = lightProjection * lightView;

                glm::mat4 view = camera.getViewMatrix();
                glm::mat4 projection = camera.getProjectionMatrix();
                globals.mvp = projection * view;
                globals.lightSpaceMatrix = lightSpaceMatrix;

                if (shouldFrustumCull) {
                    globals.indirectDrawDataBufferIndex = culledIndirectDrawDataBufferHandle.index;
                } else {
                    globals.indirectDrawDataBufferIndex = indirectDrawDataBufferHandle.index;
                }

                asyncLoader.uploadRequests.emplace_back(
                        UploadRequest{
                                .dstBuffer = gpu.getUniform(globalsRingBuffer.buffer()),
                                .data = (void *) &globals,
                        }
                );

                // shadows
                shadowPass.uniforms.lightSpaceMatrix = lightSpaceMatrix;
                shadowPass.updateUniforms(&asyncLoader);

                // frustum cull
                if (!fixedFrustum) {
                    frustumCullPass.uniforms.frustumPlanes = FrustumCullPass::getFrustumPlanes(projection * view);
                }
                frustumCullPass.uniforms.drawCount = indirectDrawDatas.size();
                frustumCullPass.updateUniforms(&asyncLoader);

                while (!asyncLoader.uploadRequests.empty()) { //todo: proper async transfer
                    asyncLoader.update();
                }

                PushConstants pc = {
                        .uniformOffset = globalsRingBuffer.buffer().index,
                };

                for (size_t i = 0; i < gltf.meshDraws.size(); i++) {
                    const MeshDraw &md = gltf.meshDraws[i];

                    indirectDrawDatas[i] = {
                            .cmd = {
                                    .indexCount = md.indexCount,
                                    .instanceCount = 1,
                                    .firstIndex = md.indexOffset,
                                    .vertexOffset = static_cast<int32_t>(md.vertexOffset),
                                    .firstInstance = 0,
                            },
                            .meshId = md.id,
                            .materialOffset = md.materialOffset,
                            .transformOffset = md.transformOffset,
                    };

                    bounds[i] = md.bounds;
                }

                memcpy(gpu.getBuffer(indirectDrawDataBufferHandle)->allocationInfo.pMappedData,
                       indirectDrawDatas.data(),
                       sizeof(IndirectDrawData) * indirectDrawDatas.size());
                memcpy(gpu.getBuffer(boundsBufferHandle)->allocationInfo.pMappedData, bounds.data(),
                       sizeof(Bounds) * bounds.size());

                gpu.newFrame();
                imgui.newFrame();

                VkCommandBuffer cmd = gpu.getCommandBuffer();

                asyncLoader.signalTextures(cmd);
                asyncLoader.signalBuffers(cmd);

                shadowPass.render(cmd,
                                  indexBufferHandle,
                                  indirectDrawDataBufferHandle, indirectDrawDatas.size());

                frustumCullPass.cull(cmd);
                //todo: implement compute queue, currently using the main queue here
                frustumCullPass.addBarriers(cmd, gpu.mainFamily, gpu.mainFamily,
                                            culledIndirectDrawDataBufferHandle,
                                            countBufferHandle);

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

                if (shouldFrustumCull) {
                    vkCmdDrawIndexedIndirectCount(cmd,
                                                  gpu.getBuffer(culledIndirectDrawDataBufferHandle)->buffer, 0,
                                                  gpu.getBuffer(countBufferHandle)->buffer, 0,
                                                  gltf.meshDraws.size(),
                                                  sizeof(IndirectDrawData));

                } else {
                    vkCmdDrawIndexedIndirect(cmd, gpu.getBuffer(indirectDrawDataBufferHandle)->buffer, 0,
                                             gltf.meshDraws.size(),
                                             sizeof(IndirectDrawData));
                }

                vkCmdEndRendering(cmd);

                uint32_t drawCount = *reinterpret_cast<uint32_t *>(gpu.getBuffer(
                        countBufferHandle)->allocationInfo.pMappedData);
                ImGui::Begin("Light");
                if (shouldFrustumCull) {
                    ImGui::Text("Draw count: %i (culled %zu)", drawCount, indirectDrawDatas.size() - drawCount);
                } else {
                    ImGui::Text("Draw count: %zu", indirectDrawDatas.size());
                }
                ImGui::Checkbox("Shadows", &shadowPass.enable);
                ImGui::Checkbox("Frustum cull", &shouldFrustumCull);
                ImGui::Checkbox("Fixed frustum", &fixedFrustum);
                ImGui::SliderFloat3("Light position",
                                    reinterpret_cast<float *>(&globals.light.position), -50.f, 50.f);
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

                gpu.present();

                if (shouldReloadPipeline) {
                    gpu.recreatePipeline(pipelineHandle, pipelineCI);
                    gpu.recreatePipeline(frustumCullPass.pipelineHandle, frustumCullPass.pipelineCI);
                    shouldReloadPipeline = false;
                }
            }
        }
    }

    void shutdown() override {
        vkDeviceWaitIdle(gpu.device);

        shadowPass.shutdown();
        frustumCullPass.shutdown();

        imgui.shutdown();

        gltf.shutdown();

        globalsRingBuffer.shutdown();

        gpu.destroyPipeline(pipelineHandle);
        gpu.destroyBuffer(positionBufferHandle);
        gpu.destroyBuffer(indexBufferHandle);
        gpu.destroyBuffer(transformBufferHandle);
        gpu.destroyBuffer(uvBufferHandle);
        gpu.destroyBuffer(textureIndicesHandle);
        gpu.destroyBuffer(materialBufferHandle);
        gpu.destroyBuffer(normalBufferHandle);
        gpu.destroyBuffer(indirectDrawDataBufferHandle);
        gpu.destroyBuffer(culledIndirectDrawDataBufferHandle);
        gpu.destroyBuffer(countBufferHandle);
        gpu.destroyBuffer(tangentBufferHandle);
        gpu.destroyBuffer(boundsBufferHandle);

        asyncLoader.shutdown();
        gpu.shutdown();
        window.shutdown();
    }

    Flare::GpuDevice gpu;
    Flare::Window window;
    Flare::AsyncLoader asyncLoader;

    bool shouldReloadPipeline = false;
    bool shouldFrustumCull = true;
    bool fixedFrustum = false;

    PipelineCI pipelineCI;
    Handle<Pipeline> pipelineHandle;

    RingBuffer globalsRingBuffer;

    Handle<Buffer> positionBufferHandle;
    Handle<Buffer> indexBufferHandle;
    Handle<Buffer> transformBufferHandle;
    Handle<Buffer> uvBufferHandle;
    Handle<Buffer> tangentBufferHandle;
    Handle<Buffer> textureIndicesHandle;
    Handle<Buffer> materialBufferHandle;
    Handle<Buffer> normalBufferHandle;

    std::vector<IndirectDrawData> indirectDrawDatas;
    Handle<Buffer> indirectDrawDataBufferHandle;
    Handle<Buffer> culledIndirectDrawDataBufferHandle;
    Handle<Buffer> countBufferHandle;

    std::vector<Bounds> bounds;
    Handle<Buffer> boundsBufferHandle;

    Camera camera;

    GltfScene gltf;

    FlareImgui imgui;

    ShadowPass shadowPass;
    FrustumCullPass frustumCullPass;
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