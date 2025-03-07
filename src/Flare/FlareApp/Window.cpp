#include "Window.h"
#include <spdlog/spdlog.h>

namespace Flare {
void Window::init(const WindowConfig &config) {
  spdlog::info("Window: Initialize");

  if (!glfwInit()) {
    spdlog::error("Window: GLFW failed to initialize");
  }

  if (!glfwVulkanSupported()) {
    spdlog::error("Window: GLFW Vulkan not supported");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindow = glfwCreateWindow(config.width, config.height, config.name,
                                nullptr, nullptr);
  if (!glfwWindow) {
    spdlog::error("Window: GLFW failed to create window");
  }

  width = config.width;
  height = config.height;
}

void Window::shutdown() {
  glfwDestroyWindow(glfwWindow);
  glfwTerminate();
  spdlog::info("Window: Shutdown");
}
} // namespace Flare
