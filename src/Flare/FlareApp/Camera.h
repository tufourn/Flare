#pragma once

#include <glm/glm.hpp>

struct GLFWwindow;

namespace Flare {

    struct Camera {
        glm::vec3 velocity = glm::vec3(0.f);
        glm::vec3 position = glm::vec3(0.f, 0.f, 5.f);
        float speed = 0.005f;

        bool shouldRotateCamera = false;
        float sensitivity = 0.0005f;
        float pitch = 0.f;
        float yaw = 0.f;

        void update();

        glm::mat4 getViewMatrix() const;

        glm::mat4 getRotationMatrix() const;

        void processMouseMovement(float xOffset, float yOffset);
        void processKeypress(int key, int action);
        void processMouseButton(int button, int action);

        static void mouseCallback(GLFWwindow* window, double xPos, double yPos);
        static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    };
}
