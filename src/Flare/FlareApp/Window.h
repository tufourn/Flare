#pragma once

#define GLFW_INCLUDE_NONE
#define VK_NO_PROTOTYPES

#include <volk.h>
#include <GLFW/glfw3.h>

namespace Flare {
    struct WindowConfig {
        WindowConfig &setWidth(uint32_t w) {
            width = w;
            return *this;
        }

        WindowConfig &setHeight(uint32_t h) {
            height = h;
            return *this;
        }

        WindowConfig &setName(const char *n) {
            name = n;
            return *this;
        }

        uint32_t width;
        uint32_t height;
        const char *name;
    };

    struct Window {
        void init(const WindowConfig &config);

        void shutdown();

        void pollEvents() const { glfwPollEvents(); }

        bool shouldClose() const { return glfwWindowShouldClose(glfwWindow); }

        bool isMinimized() const { return glfwGetWindowAttrib(glfwWindow, GLFW_ICONIFIED); }

        GLFWwindow *glfwWindow = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
    };
}

