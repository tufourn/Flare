#include "FlareApp/Application.h"
#include "FlareApp/Window.h"
#include "FlareGraphics/GpuDevice.h"
#include "FlareGraphics/ShaderCompiler.h"
#include <glm/glm.hpp>

#include "FlareGraphics/VkHelper.h"
#include "FlareGraphics/AsyncLoader.h"
#include "glm/ext/matrix_transform.hpp"
#include "FlareGraphics/GltfScene.h"
#include <stb_image.h>

using namespace Flare;

struct TriangleApp : Application {
    struct Vertex {
        glm::vec2 position;
        glm::vec2 uv;
    };

    const std::vector<Vertex> vertices = {
            {{-0.5f, -0.5f}, {0.0f, 1.0f}},  // Bottom-left
            {{0.5f,  -0.5f}, {1.0f, 1.0f}},  // Bottom-right
            {{-0.5f, 0.5f},  {0.0f, 0.0f}},  // Top-left

            {{-0.5f, 0.5f},  {0.0f, 0.0f}},  // Top-left
            {{0.5f,  -0.5f}, {1.0f, 1.0f}},  // Bottom-right
            {{0.5f,  0.5f},  {1.0f, 0.0f}}   // Top-right
    };

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

        std::vector<uint32_t> shader = shaderCompiler.compile("shaders/textured-triangle.slang");
//        std::vector<uint32_t> shader = shaderCompiler.compile("shaders/triangle_hardcode.slang");
//        std::vector<uint32_t> shader = shaderCompiler.compile("shaders/triangle_vertexbuffer.slang");

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
                                .format = VK_FORMAT_R32G32_SFLOAT,
                                .offset = static_cast<uint32_t>(offsetof(Vertex, uv)),
                        }
                );

        pipelineHandle = gpu.createPipeline(pipelineCI);

        gltf.init("assets/BoxTextured.gltf", &gpu, &asyncLoader);

        BufferCI vertexBufferCI = {
                .size = sizeof(Vertex) * vertices.size(),
                .usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        };

        vertexBufferHandle = gpu.createBuffer(vertexBufferCI);

        asyncLoader.uploadRequests.emplace_back(
                UploadRequest{
                        .dstBuffer = vertexBufferHandle,
                        .data = (void *) vertices.data(),
                }
        );
    }

    void loop() override {
        while (!window.shouldClose()) {
            window.pollEvents();

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

                VkBuffer vertexBuffers[] = {gpu.buffers.get(vertexBufferHandle)->buffer};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

                vkCmdBindDescriptorSets(cmd, pipeline->bindPoint, pipeline->pipelineLayout, 0, 1,
                                        &gpu.descriptorSets.get(gpu.bindlessDescriptorSetHandle)->descriptorSet, 0,
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

                vkCmdDraw(cmd, 6, 1, 0, 0);

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
        gpu.destroyBuffer(vertexBufferHandle);

        asyncLoader.shutdown();
        gpu.shutdown();
        window.shutdown();
    }

    Flare::GpuDevice gpu;
    Flare::Window window;
    Flare::ShaderCompiler shaderCompiler;
    Flare::AsyncLoader asyncLoader;

    Handle<Pipeline> pipelineHandle;
    Handle<Buffer> vertexBufferHandle;
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