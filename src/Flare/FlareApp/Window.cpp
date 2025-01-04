#include "Window.h"
#include "spdlog/spdlog.h"

namespace Flare {
    void Window::init(const WindowConfig& config) {
        spdlog::info("Window: Initialize");

        if (!glfwInit()) {
            spdlog::error("Window: GLFW failed to initialize");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(config.width, config.height, config.name, nullptr, nullptr);
        if (!window) {
            spdlog::error("Window: GLFW failed to create window");
        }
    }

    void Window::shutdown() {
        glfwDestroyWindow(window);
        glfwTerminate();
        spdlog::info("Window: Shutdown");
    }
}
