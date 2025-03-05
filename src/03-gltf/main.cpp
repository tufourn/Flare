#include "FlareApp/Application.h"
#include "FlareApp/Window.h"
#include "FlareApp/Camera.h"
#include "FlareGraphics/GpuDevice.h"
#include "FlareGraphics/Passes/ShadowPass.h"
#include "FlareGraphics/Passes/FrustumCullPass.h"

#include <glm/glm.hpp>

#include "FlareGraphics/VkHelper.h"
#include "FlareGraphics/GltfScene.h"
#include "FlareGraphics/FlareImgui.h"
#include "imgui.h"
#include "FlareGraphics/Passes/SkyboxPass.h"
#include "FlareGraphics/Passes/GBufferPass.h"
#include "FlareGraphics/LightData.h"
#include "FlareGraphics/Passes/LightingPass.h"

using namespace Flare;

struct TriangleApp : Application {
    struct Globals {
        glm::mat4 mvp;

        glm::vec4 cameraPos;

        uint32_t positionBufferIndex;
        uint32_t normalBufferIndex;
        uint32_t uvBufferIndex;
        uint32_t transformBufferIndex;

        uint32_t textureBufferIndex;
        uint32_t materialBufferIndex;
        uint32_t indirectDrawDataBufferIndex;
        uint32_t tangentBufferIndex;

        uint32_t shadowDepthTextureIndex;
        uint32_t shadowSamplerIndex;
        uint32_t irradianceMapIndex;
        uint32_t prefilteredCubeIndex;

        uint32_t brdfLutIndex;
        uint32_t cubemapSamplerIndex;
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

        pipelineCI.shaderStages.emplace_back(ShaderStage{"shaders/gltf.vert", VK_SHADER_STAGE_VERTEX_BIT});
        pipelineCI.shaderStages.emplace_back(ShaderStage{"shaders/gltf.frag", VK_SHADER_STAGE_FRAGMENT_BIT});
        pipelineCI.rendering.colorFormats.push_back(gpu.surfaceFormat.format);
        pipelineCI.rendering.depthFormat = VK_FORMAT_D32_SFLOAT;
        pipelineCI.depthStencil = {
                .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
                .depthTestEnable = true,
                .depthWriteEnable = true,
        };
        pipelineCI.colorBlend.addAttachment(
                {
                        .srcColor = VK_BLEND_FACTOR_SRC_ALPHA,
                        .dstColor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                        .colorOp = VK_BLEND_OP_ADD,
                        .srcAlpha = VK_BLEND_FACTOR_ONE,
                        .dstAlpha = VK_BLEND_FACTOR_ZERO,
                        .alphaOp = VK_BLEND_OP_ADD,
                        .enable = true,
                }
        );

        pipelineHandle = gpu.createPipeline(pipelineCI);

        gltf.init("assets/BoxTextured.gltf", &gpu);
//        gltf.init("assets/DamagedHelmet/DamagedHelmet.glb", &gpu);
//        gltf.init("assets/CesiumMilkTruck.gltf", &gpu);
//        gltf.init("assets/structure.glb", &gpu);
//        gltf.init("assets/Sponza/glTF/Sponza.gltf", &gpu);

        BufferCI globalsBufferCI = {
                .size = sizeof(Globals),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .name = "globals",
                .bufferType = BufferType::eUniform,
        };
        globalsRingBuffer.init(&gpu, FRAMES_IN_FLIGHT, globalsBufferCI);

        BufferCI positionsCI = {
                .initialData = gltf.positions.data(),
                .size = sizeof(glm::vec4) * gltf.positions.size(),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .name = "positions",
        };
        positionBufferHandle = gpu.createBuffer(positionsCI);

        BufferCI indicesCI = {
                .initialData = gltf.indices.data(),
                .size = sizeof(uint32_t) * gltf.indices.size(),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                .name = "indices",
        };
        indexBufferHandle = gpu.createBuffer(indicesCI);

        BufferCI textureIndicesCI = {
                .initialData = gltf.gltfTextures.data(),
                .size = sizeof(GltfTexture) * gltf.gltfTextures.size(),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .name = "textures",
        };
        textureBufferHandle = gpu.createBuffer(textureIndicesCI);

        BufferCI uvCI = {
                .initialData = gltf.uvs.data(),
                .size = sizeof(glm::vec2) * gltf.uvs.size(),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .name = "uv",
        };
        uvBufferHandle = gpu.createBuffer(uvCI);

        BufferCI tangentCI = {
                .initialData = gltf.tangents.data(),
                .size = sizeof(glm::vec4) * gltf.tangents.size(),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .name = "tangents",
        };
        tangentBufferHandle = gpu.createBuffer(tangentCI);

        BufferCI transformsCI = {
                .initialData = gltf.transforms.data(),
                .size = sizeof(glm::mat4) * gltf.transforms.size(),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .name = "transforms",
        };
        transformBufferHandle = gpu.createBuffer(transformsCI);

        BufferCI materialCI = {
                .initialData = gltf.materials.data(),
                .size = sizeof(Material) * gltf.materials.size(),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .name = "materials",
        };
        materialBufferHandle = gpu.createBuffer(materialCI);

        BufferCI normalCI = {
                .initialData = gltf.normals.data(),
                .size = sizeof(glm::vec4) * gltf.normals.size(),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .name = "normals",
        };
        normalBufferHandle = gpu.createBuffer(normalCI);

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
                .mapped = true,
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

        BufferCI lightCI = {
                .size = sizeof(LightData),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .name = "lightData",
                .bufferType = BufferType::eUniform,
        };
        lightDataRingBuffer.init(&gpu, FRAMES_IN_FLIGHT, lightCI);

        BufferCI cameraCI = {
                .size = sizeof(CameraData),
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .name = "lightData",
                .bufferType = BufferType::eUniform,
        };
        cameraDataRingBuffer.init(&gpu, FRAMES_IN_FLIGHT, cameraCI);

        shadowPass.init(&gpu);
        frustumCullPass.init(&gpu);
        skyboxPass.init(&gpu);
        skyboxPass.loadImage("assets/AllSkyFree_Sky_EpicBlueSunset_Equirect.png");
//        skyboxPass.loadImage("assets/free_hdri_sky_816.jpg");
        gBufferPass.init(&gpu);
        lightingPass.init(&gpu);

        globals.positionBufferIndex = positionBufferHandle.index;
        globals.normalBufferIndex = normalBufferHandle.index;
        globals.uvBufferIndex = uvBufferHandle.index;
        globals.transformBufferIndex = transformBufferHandle.index;

        globals.textureBufferIndex = textureBufferHandle.index;
        globals.materialBufferIndex = materialBufferHandle.index;
        globals.tangentBufferIndex = tangentBufferHandle.index;

        globals.shadowDepthTextureIndex = shadowPass.depthTextureHandle.index;
        globals.shadowSamplerIndex = shadowPass.samplerHandle.index;

        globals.irradianceMapIndex = skyboxPass.irradianceMapHandle.index;
        globals.prefilteredCubeIndex = skyboxPass.prefilteredCubeHandle.index;
        globals.brdfLutIndex = skyboxPass.brdfLutHandle.index;
        globals.cubemapSamplerIndex = skyboxPass.samplerHandle.index;

        glfwSetWindowUserPointer(window.glfwWindow, &camera);
        glfwSetCursorPosCallback(window.glfwWindow, Camera::mouseCallback);
        glfwSetKeyCallback(window.glfwWindow, Camera::keyCallback);
        glfwSetMouseButtonCallback(window.glfwWindow, Camera::mouseButtonCallback);

        imgui.init(&gpu);
    }

    void loop() override {
        while (!window.shouldClose()) {
            window.newFrame();

            if (!window.isMinimized()) {
                if (window.shouldResize) {
                    window.shouldResize = false;

                    gpu.resizeSwapchain();
                    gBufferPass.generateRenderTargets();
                    lightingPass.generateRenderTarget();
                }

                camera.update();
                camera.setAspectRatio((float) window.width / window.height);

                gpu.newFrame();
                imgui.newFrame();

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

                uint32_t meshCount = indirectDrawDatas.size();
                memcpy(gpu.getBuffer(indirectDrawDataBufferHandle)->allocationInfo.pMappedData,
                       indirectDrawDatas.data(),
                       sizeof(IndirectDrawData) * indirectDrawDatas.size());
                memcpy(gpu.getBuffer(countBufferHandle)->allocationInfo.pMappedData,
                       &meshCount,
                       sizeof(uint32_t));
                memcpy(gpu.getBuffer(boundsBufferHandle)->allocationInfo.pMappedData, bounds.data(),
                       sizeof(Bounds) * bounds.size());

                glm::mat4 view = camera.getViewMatrix();
                glm::mat4 projection = camera.getProjectionMatrix();

                // light
                lightData.updateViewProjection();
                gpu.uploadBufferData(lightDataRingBuffer.buffer(), &lightData);

                // camera
                cameraData.setMatrices(view, projection);
                gpu.uploadBufferData(cameraDataRingBuffer.buffer(), &cameraData);

                // todo: frustum cull for shadows
                // shadows
                shadowPass.setBuffers(indirectDrawDataBufferHandle, positionBufferHandle, transformBufferHandle,
                                      lightDataRingBuffer.buffer());

                // frustum cull
                Handle<Buffer> chosenIndirectDrawBufferHandle = indirectDrawDataBufferHandle;
                if (shouldFrustumCull) {
                    chosenIndirectDrawBufferHandle = culledIndirectDrawDataBufferHandle;
                    if (!fixedFrustum) {
                        frustumCullPass.uniforms.frustumPlanes = FrustumCullPass::getFrustumPlanes(projection * view);
                    }
                    frustumCullPass.updateUniforms();
                    frustumCullPass.setBuffers(indirectDrawDataBufferHandle, culledIndirectDrawDataBufferHandle,
                                               countBufferHandle, transformBufferHandle, boundsBufferHandle);
                }

                // gbuffer
                MeshDrawBuffers meshDrawBuffers = {
                        .indices = indexBufferHandle,
                        .positions = positionBufferHandle,
                        .uvs = uvBufferHandle,
                        .normals = normalBufferHandle,
                        .tangents = tangentBufferHandle,
                        .transforms = transformBufferHandle,
                        .materials = materialBufferHandle,
                        .textures = textureBufferHandle,
                        .indirectDraws = chosenIndirectDrawBufferHandle,
                        .count = countBufferHandle,
                        .drawCount = static_cast<uint32_t>(indirectDrawDatas.size()),
                };
                gBufferPass.setBuffers(meshDrawBuffers);
                gBufferPass.updateViewProjection(projection * view);

                // lighting
                lightingPass.inputs.cameraBuffer = cameraDataRingBuffer.buffer();
                lightingPass.inputs.lightBuffer = lightDataRingBuffer.buffer();
                lightingPass.inputs.gBufferAlbedo = gBufferPass.albedoTargetHandle;
                lightingPass.inputs.gBufferNormal = gBufferPass.normalTargetHandle;
                lightingPass.inputs.gBufferOcclusionMetallicRoughness = gBufferPass.occlusionMetallicRoughnessTargetHandle;
                lightingPass.inputs.gBufferEmissive = gBufferPass.emissiveTargetHandle;
                lightingPass.inputs.gBufferDepth = gBufferPass.depthTargetHandle;

                globals.mvp = projection * view;
                globals.cameraPos = glm::vec4(camera.position, 1.0);
                globals.indirectDrawDataBufferIndex = chosenIndirectDrawBufferHandle.index;

                gpu.uploadBufferData(globalsRingBuffer.buffer(), &globals);

                PushConstants pc = {
                        .uniformOffset = globalsRingBuffer.buffer().index,
                        .data0 = lightDataRingBuffer.buffer().index,
                };

                VkCommandBuffer cmd = gpu.getCommandBuffer();

                // frustum cull
                if (shouldFrustumCull) {
                    //todo: implement compute queue, currently using the main queue here
                    frustumCullPass.cull(cmd);
                    frustumCullPass.addBarriers(cmd, gpu.mainFamily, gpu.mainFamily);
                }

                // shadows
                shadowPass.render(cmd,
                                  indexBufferHandle,
                                  indirectDrawDataBufferHandle, indirectDrawDatas.size());

                // gbuffer pass
                gBufferPass.render(cmd);

                // lighting pass
                lightingPass.render(cmd);

                VkHelper::transitionImage(cmd, gpu.swapchainImages[gpu.swapchainImageIndex],
                                          VK_IMAGE_LAYOUT_UNDEFINED,
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

                VkHelper::copyImageToImage(cmd, gpu.getTexture(lightingPass.targetHandle)->image,
                                           gpu.swapchainImages[gpu.swapchainImageIndex],
                                           gpu.swapchainExtent, gpu.swapchainExtent);

                VkHelper::transitionImage(cmd, gpu.swapchainImages[gpu.swapchainImageIndex],
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);


//                VkHelper::transitionImage(cmd, gpu.swapchainImages[gpu.swapchainImageIndex],
//                                          VK_IMAGE_LAYOUT_UNDEFINED,
//                                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
//
//                VkRenderingAttachmentInfo colorAttachment = VkHelper::colorAttachment(
//                        gpu.swapchainImageViews[gpu.swapchainImageIndex]);
//
//                VkRenderingAttachmentInfo depthAttachment = VkHelper::depthAttachment(
//                        gpu.getTexture(gpu.depthTextures[gpu.swapchainImageIndex])->imageView);
//
//                VkRenderingInfo renderingInfo = VkHelper::renderingInfo(gpu.swapchainExtent, 1,
//                                                                        &colorAttachment,
//                                                                        &depthAttachment);
//
//                Pipeline *pipeline = gpu.pipelines.get(pipelineHandle);
//
//                vkCmdBeginRendering(cmd, &renderingInfo);
//                vkCmdBindPipeline(cmd, pipeline->bindPoint, pipeline->pipeline);
//
//                vkCmdBindIndexBuffer(cmd, gpu.getBuffer(indexBufferHandle)->buffer, 0, VK_INDEX_TYPE_UINT32);
//                vkCmdBindDescriptorSets(cmd, pipeline->bindPoint, pipeline->pipelineLayout, 0,
//                                        gpu.bindlessDescriptorSets.size(), gpu.bindlessDescriptorSets.data(),
//                                        0, nullptr);
//
//                VkViewport viewport = VkHelper::viewport(gpu.swapchainExtent.width, gpu.swapchainExtent.height);
//                vkCmdSetViewport(cmd, 0, 1, &viewport);
//
//                VkRect2D scissor = VkHelper::scissor(gpu.swapchainExtent.width, gpu.swapchainExtent.height);
//                vkCmdSetScissor(cmd, 0, 1, &scissor);
//
//                vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants),
//                                   &pc);
//
//                vkCmdDrawIndexedIndirectCount(cmd,
//                                              gpu.getBuffer(chosenIndirectDrawBufferHandle)->buffer, 0,
//                                              gpu.getBuffer(countBufferHandle)->buffer, 0,
//                                              gltf.meshDraws.size(),
//                                              sizeof(IndirectDrawData));
//
//                if (shouldRenderSkybox) {
//                    skyboxPass.render(cmd, projection, view);
//                }
//
//                vkCmdEndRendering(cmd);

                ImGui::Begin("Options");
                ImGui::Checkbox("Shadows", &shadowPass.enable);
                ImGui::Checkbox("Frustum cull", &shouldFrustumCull);
                ImGui::Checkbox("Fixed frustum", &fixedFrustum);
                ImGui::Checkbox("Skybox", &shouldRenderSkybox);
                ImGui::SliderFloat3("Light position",
                                    reinterpret_cast<float *>(&lightData.lightPos), -50.f, 50.f);
//                ImGui::SliderFloat("Light intensity", reinterpret_cast<float *>(&globals.light.intensity), 0.f, 100.f);
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
                lightDataRingBuffer.moveToNextBuffer();
                cameraDataRingBuffer.moveToNextBuffer();

                gpu.present();

                if (shouldReloadPipeline) {
                    gpu.recreatePipeline(pipelineHandle, pipelineCI);
                    gpu.recreatePipeline(lightingPass.pipelineHandle, lightingPass.pipelineCI);
                    shouldReloadPipeline = false;
                }
            }
        }
    }

    void shutdown() override {
        vkDeviceWaitIdle(gpu.device);

        shadowPass.shutdown();
        frustumCullPass.shutdown();
        skyboxPass.shutdown();
        gBufferPass.shutdown();
        lightingPass.shutdown();

        imgui.shutdown();

        gltf.shutdown();

        globalsRingBuffer.shutdown();
        lightDataRingBuffer.shutdown();
        cameraDataRingBuffer.shutdown();

        gpu.destroyPipeline(pipelineHandle);
        gpu.destroyBuffer(positionBufferHandle);
        gpu.destroyBuffer(indexBufferHandle);
        gpu.destroyBuffer(transformBufferHandle);
        gpu.destroyBuffer(uvBufferHandle);
        gpu.destroyBuffer(textureBufferHandle);
        gpu.destroyBuffer(materialBufferHandle);
        gpu.destroyBuffer(normalBufferHandle);
        gpu.destroyBuffer(indirectDrawDataBufferHandle);
        gpu.destroyBuffer(culledIndirectDrawDataBufferHandle);
        gpu.destroyBuffer(countBufferHandle);
        gpu.destroyBuffer(tangentBufferHandle);
        gpu.destroyBuffer(boundsBufferHandle);

        gpu.shutdown();
        window.shutdown();
    }

    Flare::GpuDevice gpu;
    Flare::Window window;

    bool shouldReloadPipeline = false;
    bool shouldFrustumCull = true;
    bool fixedFrustum = false;
    bool shouldRenderSkybox = true;

    PipelineCI pipelineCI;
    Handle<Pipeline> pipelineHandle;

    Globals globals;
    RingBuffer globalsRingBuffer;

    LightData lightData;
    RingBuffer lightDataRingBuffer;

    CameraData cameraData;
    RingBuffer cameraDataRingBuffer;

    Handle<Buffer> positionBufferHandle;
    Handle<Buffer> indexBufferHandle;
    Handle<Buffer> transformBufferHandle;
    Handle<Buffer> uvBufferHandle;
    Handle<Buffer> tangentBufferHandle;
    Handle<Buffer> textureBufferHandle;
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
    SkyboxPass skyboxPass;
    GBufferPass gBufferPass;
    LightingPass lightingPass;
};

int main() {
    Flare::ApplicationConfig appConfig{};
    appConfig
            .setWidth(1280)
            .setHeight(720)
            .setName("Vulkan");

    TriangleApp app;
    app.run(appConfig);
}