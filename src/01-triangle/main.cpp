#include "FlareApp/Application.h"
#include "FlareApp/Window.h"

struct TriangleApp : Flare::Application {
    void init(const Flare::ApplicationConfig& appConfig) override {
        Flare::WindowConfig windowConfig {};
        windowConfig
            .setWidth(appConfig.width)
            .setHeight(appConfig.height)
            .setName(appConfig.name);

        window.init(windowConfig);
    }

    void loop() override {
        while (!window.shouldClose()) {
            window.pollEvents();
        }
    }

    void shutdown() override {
        window.shutdown();
    }

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