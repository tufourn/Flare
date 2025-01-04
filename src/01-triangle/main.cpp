#include "FlareApp/Application.h"
#include "FlareApp/Window.h"
#include "FlareGraphics/GpuDevice.h"

struct TriangleApp : Flare::Application {
    void init(const Flare::ApplicationConfig& appConfig) override {
        Flare::WindowConfig windowConfig {};
        windowConfig
            .setWidth(appConfig.width)
            .setHeight(appConfig.height)
            .setName(appConfig.name);

        window.init(windowConfig);

        Flare::GpuDeviceCreateInfo gpuDeviceCI {
            .width = window.width,
            .height = window.height,
            .glfwWindow = window.glfwWindow,
        };

        gpu.init(gpuDeviceCI);
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