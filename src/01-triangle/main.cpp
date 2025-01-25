#include "FlareApp/Application.h"
#include "FlareApp/Window.h"
#include "FlareGraphics/GpuDevice.h"
#include "FlareGraphics/ShaderCompiler.h"
#include <glm/glm.hpp>

#include "FlareGraphics/VkHelper.h"

using namespace Flare;

struct TriangleApp : Application {
    struct Vertex {
        glm::vec2 position;
        glm::vec3 color;
    };

    const std::vector<Vertex> vertices = {
            {{0.0f,  -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f,  0.5f},  {0.0f, 1.0f, 0.0f}},
            {{-0.5f, 0.5f},  {0.0f, 0.0f, 1.0f}}
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
        shaderCompiler.init();

//        std::vector<uint32_t> shader = shaderCompiler.compile("shaders/shaders.slang");
//        std::vector<uint32_t> shader = shaderCompiler.compile("shaders/triangle_hardcode.slang");
        std::vector<uint32_t> shader = shaderCompiler.compile("shaders/triangle_vertexbuffer.slang");

        Flare::ReflectOutput reflection;
        std::vector<ShaderExecModel> execModels = {
                {VK_SHADER_STAGE_VERTEX_BIT,   "main"},
                {VK_SHADER_STAGE_FRAGMENT_BIT, "main"},
        };

        PipelineCI pipelineCI;
        pipelineCI.shaderStages.addBinary({execModels, shader});
        pipelineCI.rendering.colorFormats.push_back(gpu.surfaceFormat.format);
        pipelineCI.vertexInput
                .addBinding(
                        VkVertexInputBindingDescription{
                                .binding = 0,
                                .stride = sizeof(Vertex),
                                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                        }
                )
                .addAttribute(
                        VkVertexInputAttributeDescription{
                                .location = 0,
                                .binding = 0,
                                .format = VK_FORMAT_R32G32_SFLOAT,
                                .offset = static_cast<uint32_t>(offsetof(Vertex, position)),
                        }
                )
                .addAttribute(
                        VkVertexInputAttributeDescription{
                                .location = 1,
                                .binding = 0,
                                .format = VK_FORMAT_R32G32B32_SFLOAT,
                                .offset = static_cast<uint32_t>(offsetof(Vertex, color)),
                        }
                );

        pipeline = gpu.createPipeline(pipelineCI);

        BufferCI vertexBufferCI = {
                .size = sizeof(Vertex) * vertices.size(),
                .usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .mapped = true,
                .initialData = (void *) vertices.data(),
        };

        vertexBuffer = gpu.createBuffer(vertexBufferCI);

    }

    void loop() override {
        while (!window.shouldClose()) {
            window.pollEvents();

            if (!window.isMinimized()) {
                gpu.newFrame();

                VkCommandBuffer cmd = gpu.getCommandBuffer();
                VkHelper::transitionImage(cmd, gpu.swapchainImages[gpu.swapchainImageIndex],
                                          VK_IMAGE_LAYOUT_UNDEFINED,
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
                        .renderArea = {VkOffset2D{0, 0}, gpu.swapchainExtent},
                        .layerCount = 1,
                        .viewMask = 0,
                        .colorAttachmentCount = 1,
                        .pColorAttachments = &colorAttachment,
                        .pDepthAttachment = nullptr,
                        .pStencilAttachment = nullptr,
                };

                vkCmdBeginRendering(cmd, &renderingInfo);
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gpu.pipelines.get(pipeline)->pipeline);

                VkBuffer vertexBuffers[] = {gpu.buffers.get(vertexBuffer)->buffer};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

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

                vkCmdDraw(cmd, 3, 1, 0, 0);

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
        gpu.destroyPipeline(pipeline);
        gpu.destroyBuffer(vertexBuffer);
        gpu.shutdown();
        window.shutdown();
    }

    Flare::GpuDevice gpu;
    Flare::Window window;
    Flare::ShaderCompiler shaderCompiler;

    Handle<Pipeline> pipeline;
    Handle<Buffer> vertexBuffer;
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