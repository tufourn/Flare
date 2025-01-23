#include "FlareApp/Application.h"
#include "FlareApp/Window.h"
#include "FlareGraphics/GpuDevice.h"
#include "FlareGraphics/ShaderCompiler.h"

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

        auto pipeline = gpu.createPipeline(pipelineCI);

        gpu.destroyPipeline(pipeline);
    }

    void loop() override {
        while (!window.shouldClose()) {
            window.pollEvents();
            gpu.newFrame();

            gpu.present();
        }
    }

    void shutdown() override {
        gpu.shutdown();
        window.shutdown();
    }

    Flare::GpuDevice gpu;
    Flare::Window window;
    Flare::ShaderCompiler shaderCompiler;
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