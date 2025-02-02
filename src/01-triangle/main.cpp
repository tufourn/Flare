#include "FlareApp/Application.h"
#include "FlareApp/Window.h"
#include "FlareGraphics/GpuDevice.h"
#include "FlareGraphics/ShaderCompiler.h"
#include <glm/glm.hpp>

#include "FlareGraphics/VkHelper.h"
#include "FlareGraphics/AsyncLoader.h"
#include "glm/ext/matrix_transform.hpp"

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

    struct Uniform {
        glm::mat4 mvp = glm::rotate(glm::mat4(1.f), glm::radians(180.f), glm::vec3(0.0f, 0.0f, 1.0f));
    };

    const Uniform uniform = {};

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

        std::vector<uint32_t> shader = shaderCompiler.compile("shaders/shaders.slang");
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
                                .format = VK_FORMAT_R32G32B32_SFLOAT,
                                .offset = static_cast<uint32_t>(offsetof(Vertex, color)),
                        }
                );

        pipelineHandle = gpu.createPipeline(pipelineCI);

        BufferCI vertexBufferCI = {
                .size = sizeof(Vertex) * vertices.size(),
                .usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        };

        vertexBufferHandle = gpu.createBuffer(vertexBufferCI);

        BufferCI uniformBufferCI = {
                .size = sizeof(Uniform),
                .usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        };

        uniformBufferHandle = gpu.createBuffer(uniformBufferCI);

        asyncLoader.uploadRequests.emplace_back(
                UploadRequest{
                        .dstBuffer = vertexBufferHandle,
                        .data = (void *) vertices.data(),
                }
        );

        asyncLoader.uploadRequests.emplace_back(
                UploadRequest{
                        .dstBuffer = uniformBufferHandle,
                        .data = (void *) &uniform,
                }
        );

        Pipeline *pipeline = gpu.pipelines.get(pipelineHandle);

        DescriptorSetCI descSetCI = {
                .layout = pipeline->descriptorSetLayoutHandles[0],
        };

        descSetCI.addBuffer(uniformBufferHandle, 0);

        descriptorSetHandle = gpu.createDescriptorSet(descSetCI);

        const char *texturePath = "uv1.png";
        int width, height, components;
        stbi_info(texturePath, &width, &height, &components);

        TextureCI textureCI = {
                .width = static_cast<uint16_t>(width),
                .height = static_cast<uint16_t>(height),
                .depth = 1,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .type = VK_IMAGE_TYPE_2D,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
        };

        textureHandle = gpu.createTexture(textureCI);
        asyncLoader.fileRequests.emplace_back(
                FileRequest{
                        .path = texturePath,
                        .texture = textureHandle,
                }
        );
    }

    void loop() override {
        while (!window.shouldClose()) {
            window.pollEvents();

            if (!window.isMinimized()) {
                asyncLoader.update();

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

                Pipeline *pipeline = gpu.pipelines.get(pipelineHandle);

                vkCmdBeginRendering(cmd, &renderingInfo);
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);

                VkBuffer vertexBuffers[] = {gpu.buffers.get(vertexBufferHandle)->buffer};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

                DescriptorSet *descSet = gpu.descriptorSets.get(descriptorSetHandle);
                VkDescriptorSet vkDescSet = descSet->descriptorSet;

                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, 0, 1,
                                        &vkDescSet, 0, nullptr);

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

        gpu.destroyPipeline(pipelineHandle);
        gpu.destroyBuffer(vertexBufferHandle);
        gpu.destroyBuffer(uniformBufferHandle);

        gpu.destroyTexture(textureHandle);

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
    Handle<Buffer> uniformBufferHandle;
    Handle<Texture> textureHandle;
    Handle<DescriptorSet> descriptorSetHandle;
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