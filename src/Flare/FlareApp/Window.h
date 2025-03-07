#pragma once

#define GLFW_INCLUDE_NONE
#define VK_NO_PROTOTYPES

#include <GLFW/glfw3.h>
#include <volk.h>

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

  void newFrame() {
    glfwPollEvents();

    int newWidth, newHeight;
    glfwGetWindowSize(glfwWindow, &newWidth, &newHeight);
    if (newWidth != width || newHeight != height) {
      shouldResize = true;
    }
    width = newWidth;
    height = newHeight;
  }

  bool shouldClose() const { return glfwWindowShouldClose(glfwWindow); }

  bool isMinimized() const {
    return glfwGetWindowAttrib(glfwWindow, GLFW_ICONIFIED);
  }

  GLFWwindow *glfwWindow = nullptr;
  int width = 0;
  int height = 0;
  bool shouldResize = false;
};
} // namespace Flare
