#include <glm/glm.hpp>

#include "FlareApp/Application.h"
#include "FlareApp/Camera.h"
#include "FlareApp/Window.h"
#include "FlareGraphics/FlareImgui.h"
#include "FlareGraphics/GltfScene.h"
#include "FlareGraphics/GpuDevice.h"
#include "FlareGraphics/LightData.h"
#include "FlareGraphics/ModelManager.h"
#include "FlareGraphics/Passes/FrustumCullPass.h"
#include "FlareGraphics/Passes/GBufferPass.h"
#include "FlareGraphics/Passes/LightingPass.h"
#include "FlareGraphics/Passes/ShadowPass.h"
#include "FlareGraphics/Passes/SkyboxPass.h"
#include "FlareGraphics/Passes/DrawBoundsPass.h"
#include "imgui.h"

using namespace Flare;

struct TriangleApp : Application {
  void init(const ApplicationConfig &appConfig) override {
    WindowConfig windowConfig{};
    windowConfig.setWidth(appConfig.width)
        .setHeight(appConfig.height)
        .setName(appConfig.name);

    window.init(windowConfig);

    GpuDeviceCreateInfo gpuDeviceCI{
        .glfwWindow = window.glfwWindow,
    };

    gpu.init(gpuDeviceCI);

    modelManager.init(&gpu, 100, 100);
    culledIndirectDrawRingBuffer.init(&gpu, FRAMES_IN_FLIGHT);

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
        .name = "camera",
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
    drawBoundsPass.init(&gpu);

    glfwSetWindowUserPointer(window.glfwWindow, &camera);
    glfwSetCursorPosCallback(window.glfwWindow, Camera::mouseCallback);
    glfwSetKeyCallback(window.glfwWindow, Camera::keyCallback);
    glfwSetMouseButtonCallback(window.glfwWindow, Camera::mouseButtonCallback);

    imgui.init(&gpu);
  }

  void loop() override {
    while (!window.shouldClose()) {
      window.newFrame();
      modelManager.newFrame();

      if (!window.isMinimized()) {
        if (window.shouldResize) {
          window.shouldResize = false;

          gpu.resizeSwapchain();
          gBufferPass.generateRenderTargets();
        }

        camera.update();
        camera.setAspectRatio((float)window.width / window.height);

        gpu.newFrame();
        imgui.newFrame();

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
        ShadowInputs shadowPassInputs = {
            .positionBuffer = modelManager.positionBufferHandle,
            .transformBuffer = modelManager.transformRingBuffer.buffer(),
            .lightBuffer = lightDataRingBuffer.buffer(),

            .indexBuffer = modelManager.indexBufferHandle,
            .indirectDrawBuffer =
                modelManager.indirectDrawDataRingBuffer.buffer(),
            .countBuffer = modelManager.countRingBuffer.buffer(),
            .maxDrawCount = modelManager.count,
        };
        shadowPass.setInputs(shadowPassInputs);

        // frustum cull
        if (modelManager.shouldDraw() &&
            (!culledIndirectDrawRingBuffer.buffer().isValid() ||
             gpu.getBuffer(culledIndirectDrawRingBuffer.buffer())->size <
                 gpu.getBuffer(modelManager.indirectDrawDataRingBuffer.buffer())
                     ->size)) {
          BufferCI indirectDrawsCI = {
              .size = sizeof(IndirectDrawData) * modelManager.count,
              .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
              .name = "culled indirect draws",
          };
          culledIndirectDrawRingBuffer.createBuffer(indirectDrawsCI);
        }

        Handle<Buffer> chosenIndirectDrawBufferHandle =
            modelManager.indirectDrawDataRingBuffer.buffer();

        FrustumCullInputs frustumCullInputs = {
            .viewProjection = projection * view,
            .inputIndirectDrawBuffer =
                modelManager.indirectDrawDataRingBuffer.buffer(),
            .outputIndirectDrawBuffer = culledIndirectDrawRingBuffer.buffer(),
            .countBuffer = modelManager.countRingBuffer.buffer(),
            .boundsBuffer = modelManager.boundsRingBuffer.buffer(),
            .transformBuffer = modelManager.transformRingBuffer.buffer(),
            .maxDrawCount = modelManager.count,
        };
        if (shouldFrustumCull) {
          chosenIndirectDrawBufferHandle =
              culledIndirectDrawRingBuffer.buffer();
          frustumCullPass.setInputs(frustumCullInputs);
        }

        // gbuffer
        GBufferInputs gBufferInputs = {
            .viewProjection = projection * view,
            .meshDrawBuffers =
                {
                    .indices = modelManager.indexBufferHandle,
                    .positions = modelManager.positionBufferHandle,
                    .uvs = modelManager.uvBufferHandle,
                    .normals = modelManager.normalBufferHandle,
                    .tangents = modelManager.tangentBufferHandle,
                    .transforms = modelManager.transformRingBuffer.buffer(),
                    .materials = modelManager.materialBufferHandle,
                    .textures = modelManager.textureIndexBufferHandle,
                    .indirectDraws = chosenIndirectDrawBufferHandle,
                    .count = modelManager.countRingBuffer.buffer(),
                    .drawCount = modelManager.count,
                },
        };
        gBufferPass.setInputs(gBufferInputs);

        // lighting
        LightingPassInputs lightingPassInputs = {
            .drawTexture = gpu.drawTexture,
            .cameraBuffer = cameraDataRingBuffer.buffer(),
            .lightBuffer = lightDataRingBuffer.buffer(),

            .gBufferAlbedo = gBufferPass.albedoTargetHandle,
            .gBufferNormal = gBufferPass.normalTargetHandle,
            .gBufferOcclusionMetallicRoughness =
                gBufferPass.occlusionMetallicRoughnessTargetHandle,
            .gBufferEmissive = gBufferPass.emissiveTargetHandle,
            .gBufferDepth = gBufferPass.depthTargetHandle,

            .shadowMap = shadowPass.depthTextureHandle,
            .shadowSampler = shadowPass.samplerHandle,

            .irradianceMap = skyboxPass.irradianceMapHandle,
            .prefilteredCube = skyboxPass.prefilteredCubeHandle,
            .brdfLut = skyboxPass.brdfLutHandle,
        };
        lightingPass.setInputs(lightingPassInputs);

        SkyboxInputs skyboxInputs = {
            .projection = projection,
            .view = view,
            .colorAttachment = gpu.drawTexture,
            .depthAttachment = gBufferPass.depthTargetHandle,
        };
        skyboxPass.setInputs(skyboxInputs);

        DrawBoundsInputs drawBoundsInputs = {
          .viewProjection = projection * view,
          .boundsBuffer = modelManager.boundsRingBuffer.buffer(),
          .transformBuffer = modelManager.transformRingBuffer.buffer(),
          .count = modelManager.count,
        };
        drawBoundsPass.setInputs(drawBoundsInputs);

        VkCommandBuffer cmd = gpu.getCommandBuffer();
        gpu.transitionDrawTextureToColorAttachment(cmd);

        // todo: separate frustum cull for shadows
        // shadows
        shadowPass.render(cmd);

        // frustum cull
        if (shouldFrustumCull) {
          // todo: implement compute queue, currently using the main queue
          frustumCullPass.cull(cmd);
          frustumCullPass.addBarriers(cmd, gpu.mainFamily, gpu.mainFamily);
        }

        // gbuffer pass
        gBufferPass.render(cmd);

        if (modelManager.count > 0) {
          // lighting pass
          lightingPass.render(cmd);
        }

        // skybox pass
        if (shouldRenderSkybox) {
          skyboxPass.render(cmd);
        }

        if (shouldDrawBounds) {
          drawBoundsPass.render(cmd);
        }

        modelManager.drawImguiMenu();

        ImGui::Begin("Options");

        ImGui::Checkbox("Shadows", &shadowPass.enable);
        ImGui::Checkbox("Frustum cull", &shouldFrustumCull);
        ImGui::Checkbox("Fixed frustum", &frustumCullPass.fixedFrustum);
        ImGui::Checkbox("Skybox", &shouldRenderSkybox);
        ImGui::Checkbox("Bounds", &shouldDrawBounds);
        ImGui::SliderFloat3("Light position",
                            reinterpret_cast<float *>(&lightData.lightPos),
                            -50.f, 50.f);
        //                ImGui::SliderFloat("Light intensity",
        //                reinterpret_cast<float *>(&globals.light.intensity),
        //                0.f, 100.f);
        if (ImGui::Button("Reload pipeline")) {
          shouldReloadPipeline = true;
        }
        ImGui::End();

        ImGui::Render();
        imgui.draw(cmd, gpu.getTexture(gpu.drawTexture)->imageView);

        gpu.transitionDrawTextureToTransferSrc(cmd);
        gpu.transitionSwapchainTextureToTransferDst(cmd);
        gpu.copyDrawTextureToSwapchain(cmd);
        gpu.transitionSwapchainTextureToPresentSrc(cmd);

        vkEndCommandBuffer(cmd);

        culledIndirectDrawRingBuffer.moveToNextBuffer();
        lightDataRingBuffer.moveToNextBuffer();
        cameraDataRingBuffer.moveToNextBuffer();

        gpu.present();

        if (shouldReloadPipeline) {
          gpu.recreatePipeline(lightingPass.pipelineHandle,
                               lightingPass.pipelineCI);
          shouldReloadPipeline = false;
        }
      }
    }
  }

  void shutdown() override {
    vkDeviceWaitIdle(gpu.device);

    modelManager.shutdown();
    culledIndirectDrawRingBuffer.shutdown();

    shadowPass.shutdown();
    frustumCullPass.shutdown();
    skyboxPass.shutdown();
    gBufferPass.shutdown();
    lightingPass.shutdown();
    drawBoundsPass.shutdown();

    imgui.shutdown();

    lightDataRingBuffer.shutdown();
    cameraDataRingBuffer.shutdown();

    gpu.shutdown();
    window.shutdown();
  }

  Flare::GpuDevice gpu;
  Flare::Window window;

  bool shouldReloadPipeline = false;
  bool shouldFrustumCull = true;
  bool shouldRenderSkybox = true;
  bool shouldDrawBounds = false;

  LightData lightData;
  RingBuffer lightDataRingBuffer;

  CameraData cameraData;
  RingBuffer cameraDataRingBuffer;

  ModelManager modelManager;

  RingBuffer culledIndirectDrawRingBuffer;

  Camera camera;

  FlareImgui imgui;

  ShadowPass shadowPass;
  FrustumCullPass frustumCullPass;
  SkyboxPass skyboxPass;
  GBufferPass gBufferPass;
  LightingPass lightingPass;
  DrawBoundsPass drawBoundsPass;
};

int main() {
  Flare::ApplicationConfig appConfig{};
  appConfig.setWidth(1280).setHeight(720).setName("Vulkan");

  TriangleApp app;
  app.run(appConfig);
}