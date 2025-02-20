#include "FlareApp/Application.h"
#include "FlareApp/Window.h"
#include "FlareApp/Camera.h"
#include "FlareGraphics/GpuDevice.h"
#include "FlareGraphics/ShaderCompiler.h"

#include <glm/glm.hpp>

#include "FlareGraphics/VkHelper.h"
#include "FlareGraphics/AsyncLoader.h"
#include "glm/ext/matrix_transform.hpp"
#include "FlareGraphics/GltfScene.h"
#include "FlareGraphics/RingBuffer.h"
#include "FlareGraphics/FlareImgui.h"
#include "imgui.h"

using namespace Flare;

struct TriangleApp : Application {
    struct Globals {
        glm::mat4 mvp;

        uint32_t positionBufferIndex;
        uint32_t normalBufferIndex;
        uint32_t uvBufferIndex;
        uint32_t transformBufferIndex;

        uint32_t textureBufferIndex;
        uint32_t materialBufferIndex;
        uint32_t meshDrawBufferIndex;
        float pad;

        Light light;
    };

    Globals globals;

    struct GpuMeshDraw {
        uint32_t transformOffset;
        uint32_t materialOffset;
    };

    struct IndirectDrawCommand {
        VkDrawIndexedIndirectCommand cmd;
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

        std::vector<uint32_t> vertShader = shaderCompiler.compileGLSL("shaders/gltf.vert");
        std::vector<uint32_t> fragShader = shaderCompiler.compileGLSL("shaders/gltf.frag");

        PipelineCI pipelineCI;
        pipelineCI.shaderStages.addBinary({VK_SHADER_STAGE_VERTEX_BIT, vertShader});
        pipelineCI.shaderStages.addBinary({VK_SHADER_STAGE_FRAGMENT_BIT, fragShader});
        pipelineCI.rendering.colorFormats.push_back(gpu.surfaceFormat.format);
        pipelineCI.rendering.depthFormat = VK_FORMAT_D32_SFLOAT;
        pipelineCI.depthStencil = {
                .depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
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
                .size = sizeof(GpuMeshDraw) * gltf.meshDraws.size(),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .name = "meshdraws"
        };

        gpuMeshDraws.reserve(gltf.meshDraws.size());
        for (auto &meshDraw: gltf.meshDraws) {
            gpuMeshDraws.emplace_back(GpuMeshDraw{
                    .transformOffset = meshDraw.transformOffset,
                    .materialOffset = meshDraw.materialOffset,
            });
        }

        meshDrawBufferHandle = gpu.createBuffer(meshDrawCI);
        asyncLoader.uploadRequests.emplace_back(
                UploadRequest{
                        .dstBuffer = gpu.getBuffer(meshDrawBufferHandle),
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

        globals.positionBufferIndex = positionBufferHandle.index;
        globals.normalBufferIndex = normalBufferHandle.index;
        globals.uvBufferIndex = uvBufferHandle.index;
        globals.transformBufferIndex = transformBufferHandle.index;
        globals.textureBufferIndex = textureIndicesHandle.index;
        globals.materialBufferIndex = materialBufferHandle.index;
        globals.meshDrawBufferIndex = meshDrawBufferHandle.index;

        globals.light = {
                .position = {0.f, 4.f, 0.f},
                .radius = 1.f,
                .color = {1.f, 1.f, 1.f},
                .intensity = 1.f,
        };

        glfwSetWindowUserPointer(window.glfwWindow, &camera);
        glfwSetCursorPosCallback(window.glfwWindow, Camera::mouseCallback);
        glfwSetKeyCallback(window.glfwWindow, Camera::keyCallback);
        glfwSetMouseButtonCallback(window.glfwWindow, Camera::mouseButtonCallback);

        imgui.init(&gpu);
    }

    void loop() override {
        while (!window.shouldClose()) {
            window.newFrame();
            imgui.newFrame();

            if (!window.isMinimized()) {
                camera.update();

                glm::mat4 view = camera.getViewMatrix();
                glm::mat4 projection = glm::perspectiveZO(glm::radians(45.f),
                                                          static_cast<float>(window.width) / window.height, 1e9f,
                                                          0.001f);
                projection[1][1] *= -1;
                globals.mvp = projection * view;

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
                        .bindingsOffset = globalsRingBuffer.buffer().index,
                };

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

                VkRenderingAttachmentInfo depthAttachment = {
                        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                        .pNext = nullptr,
                        .imageView = gpu.textures.get(gpu.depthTextures[gpu.swapchainImageIndex])->imageView,
                        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                        .clearValue = {
                                .depthStencil = {
                                        .depth = 0.f,
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

                vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants),
                                   &pc);

                memcpy(gpu.getBuffer(indirectDrawBufferHandle)->allocationInfo.pMappedData, indirectDraws.data(),
                       sizeof(IndirectDrawCommand) * indirectDraws.size());
                vkCmdDrawIndexedIndirect(cmd, gpu.getBuffer(indirectDrawBufferHandle)->buffer, 0, gltf.meshDraws.size(),
                                         sizeof(IndirectDrawCommand));

                vkCmdEndRendering(cmd);

                ImGui::Begin("Light");
                ImGui::SliderFloat3("Light", reinterpret_cast<float *>(&globals.light.position), -10.f, 10.f);
                ImGui::End();

                ImGui::Render();
                imgui.draw(cmd, gpu.swapchainImageViews[gpu.swapchainImageIndex]);

                VkHelper::transitionImage(cmd, gpu.swapchainImages[gpu.swapchainImageIndex],
                                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

                vkEndCommandBuffer(cmd);

                globalsRingBuffer.moveToNextBuffer();
                gpu.present();
//                spdlog::info("{}", gpu.absoluteFrame);
            }
        }
    }

    void shutdown() override {
        vkDeviceWaitIdle(gpu.device);

        imgui.shutdown();

        gltf.shutdown();

        globalsRingBuffer.shutdown();

        gpu.destroyPipeline(pipelineHandle);
        gpu.destroyBuffer(positionBufferHandle);
        gpu.destroyBuffer(indexBufferHandle);
        gpu.destroyBuffer(transformBufferHandle);
        gpu.destroyBuffer(uvBufferHandle);
        gpu.destroyBuffer(meshDrawBufferHandle);
        gpu.destroyBuffer(textureIndicesHandle);
        gpu.destroyBuffer(materialBufferHandle);
        gpu.destroyBuffer(normalBufferHandle);
        gpu.destroyBuffer(indirectDrawBufferHandle);

        asyncLoader.shutdown();
        gpu.shutdown();
        window.shutdown();
    }

    Flare::GpuDevice gpu;
    Flare::Window window;
    Flare::ShaderCompiler shaderCompiler;
    Flare::AsyncLoader asyncLoader;

    Handle<Pipeline> pipelineHandle;

    RingBuffer globalsRingBuffer;
    Handle<Buffer> positionBufferHandle;
    Handle<Buffer> indexBufferHandle;
    Handle<Buffer> transformBufferHandle;
    Handle<Buffer> uvBufferHandle;
    Handle<Buffer> meshDrawBufferHandle;
    Handle<Buffer> textureIndicesHandle;
    Handle<Buffer> materialBufferHandle;
    Handle<Buffer> normalBufferHandle;
    std::vector<IndirectDrawCommand> indirectDraws;
    Handle<Buffer> indirectDrawBufferHandle;
    std::vector<GpuMeshDraw> gpuMeshDraws;

    Camera camera;

    GltfScene gltf;

    FlareImgui imgui;
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