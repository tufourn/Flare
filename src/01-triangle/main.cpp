#include "FlareApp/Application.h"
#include "FlareApp/Window.h"
#include "FlareGraphics/GpuDevice.h"
#include "FlareGraphics/ShaderCompiler.h"

struct TriangleApp : Flare::Application {
    void init(const Flare::ApplicationConfig& appConfig) override {
        Flare::WindowConfig windowConfig {};
        windowConfig
            .setWidth(appConfig.width)
            .setHeight(appConfig.height)
            .setName(appConfig.name);

        window.init(windowConfig);

        Flare::GpuDeviceCreateInfo gpuDeviceCI {
            .glfwWindow = window.glfwWindow,
        };

        gpu.init(gpuDeviceCI);

        shaderCompiler.init();

        std::vector<uint32_t> shader = shaderCompiler.compile("shaders/shaders.slang");

        Flare::ReflectOutput reflection;
        shaderCompiler.reflect(reflection, shader, Flare::ExecModel::eVertex | Flare::ExecModel::eFragment);
    }

    void loop() override {
        while (!window.shouldClose()) {
            window.pollEvents();
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
    Flare::ApplicationConfig appConfig {};
    appConfig
        .setWidth(800)
        .setHeight(600)
        .setName("Triangle");

    TriangleApp app;
    app.run(appConfig);
}