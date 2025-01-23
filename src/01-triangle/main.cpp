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
        pipelineCI.rendering.colorFormats.push_back(gpu.surfaceFormat.format);

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
                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            VkRenderingAttachmentInfo colorAttachment = {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .pNext = nullptr,
                    .imageView = gpu.swapchainImageViews[gpu.swapchainImageIndex],
                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            };

            VkRenderingInfo renderingInfo = {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                    .pNext = nullptr,
                    .flags = 0,
                    .renderArea = { VkOffset2D{0, 0}, gpu.swapchainExtent },
                    .layerCount = 1,
                    .viewMask = 0,
                    .colorAttachmentCount = 1,
                    .pColorAttachments = &colorAttachment,
                    .pDepthAttachment = nullptr,
                    .pStencilAttachment = nullptr,
            };

            vkCmdBeginRendering(cmd, &renderingInfo);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gpu.pipelines.get(pipeline)->pipeline);

            VkViewport viewport = {
                    .x = 0,
                    .y = 0,
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

            vkCmdDraw(cmd, 3, 1, 0, 0);

            vkCmdEndRendering(cmd);

            VkHelper::transitionImage(cmd, gpu.swapchainImages[gpu.swapchainImageIndex],
                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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