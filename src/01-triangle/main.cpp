#include "FlareApp/Application.h"
#include "FlareApp/Window.h"
#include "FlareGraphics/GpuDevice.h"
#include "FlareGraphics/ShaderCompiler.h"

#include "FlareGraphics/VkHelper.h"

using namespace Flare;

struct TriangleApp : Application {
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
        shaderCompiler.init();

//        std::vector<uint32_t> shader = shaderCompiler.compile("shaders/shaders.slang");
        std::vector<uint32_t> shader = shaderCompiler.compile("shaders/triangle_hardcode.slang");

        Flare::ReflectOutput reflection;
        std::vector<ShaderExecModel> execModels = {
                {VK_SHADER_STAGE_VERTEX_BIT,   "main"},
                {VK_SHADER_STAGE_FRAGMENT_BIT, "main"},
        };

        PipelineCI pipelineCI;
        pipelineCI.shaderStages.addBinary({execModels, shader});

        pipeline = gpu.createPipeline(pipelineCI);
    }

    void loop() override {
        while (!window.shouldClose()) {
            window.pollEvents();
            gpu.newFrame();

            VkCommandBuffer cmd = gpu.getCommandBuffer();
            VkHelper::transitionImage(cmd, gpu.swapchainImages[gpu.swapchainImageIndex],
                                      VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_GENERAL);

            VkClearColorValue clearValue;
            float flash = std::abs(std::sin(gpu.absoluteFrame / 1200.f));
            clearValue = {{0.0f, 0.0f, flash, 1.0f}};

            VkImageSubresourceRange clearRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = VK_REMAINING_MIP_LEVELS,
                    .baseArrayLayer = 0,
                    .layerCount = VK_REMAINING_ARRAY_LAYERS,
            };

            vkCmdClearColorImage(cmd, gpu.swapchainImages[gpu.swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL,
                                 &clearValue, 1, &clearRange);

            VkHelper::transitionImage(cmd, gpu.swapchainImages[gpu.swapchainImageIndex],
                                      VK_IMAGE_LAYOUT_GENERAL,
                                      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

            vkEndCommandBuffer(cmd);

            gpu.present();
        }
    }

    void shutdown() override {
        gpu.destroyPipeline(pipeline);
        gpu.shutdown();
        window.shutdown();
    }

    Flare::GpuDevice gpu;
    Flare::Window window;
    Flare::ShaderCompiler shaderCompiler;

    Handle<Pipeline> pipeline;
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