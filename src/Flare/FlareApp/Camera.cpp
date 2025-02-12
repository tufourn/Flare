#include "Camera.h"
#include "GLFW/glfw3.h"
#include "spdlog/spdlog.h"

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Flare {
    void Camera::update() {
        position += glm::vec3(getRotationMatrix() * glm::vec4(velocity, 0.f) * speed);
    }

    glm::mat4 Camera::getViewMatrix() const {
        glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position);
        glm::mat4 cameraRotation = getRotationMatrix();

        return glm::inverse(cameraTranslation * cameraRotation);
    }

    glm::mat4 Camera::getRotationMatrix() const {
        glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3(1.f, 0.f, 0.f));
        glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3(0.f, -1.f, 0.f));

        return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
    }

    void Camera::processMouseMovement(float xOffset, float yOffset) {
        if (!shouldRotateCamera) {
            return;
        }

        xOffset *= sensitivity;
        yOffset *= sensitivity;

        yaw -= xOffset;
        pitch -= yOffset;

        if (pitch >= 90.f) {
            pitch = 90.f;
        }

        if (pitch < -90.f) {
            pitch = -90.f;
        }
    }

    void Camera::mouseCallback(GLFWwindow *window, double xPos, double yPos) {
        static double lastX = xPos;
        static double lastY = yPos;

        Camera *camera = reinterpret_cast<Camera *>(glfwGetWindowUserPointer(window));

        double xOffset = xPos - lastX;
        double yOffset = lastY - yPos;

        lastX = xPos;
        lastY = yPos;

        camera->processMouseMovement(xOffset, yOffset);
    }

    void Camera::processKeypress(int key, int action) {
        if (action == GLFW_PRESS) {
            switch (key) {
                case GLFW_KEY_R: // reset camera
                    position = glm::vec3(0.f, 0.f, 5.f);
                    pitch = 0.f;
                    yaw = 0.f;
                    break;
                case GLFW_KEY_W:
                    velocity.z = -1;
                    break;
                case GLFW_KEY_S:
                    velocity.z = 1;
                    break;
                case GLFW_KEY_A:
                    velocity.x = -1;
                    break;
                case GLFW_KEY_D:
                    velocity.x = 1;
                    break;
                case GLFW_KEY_SPACE:
                    velocity.y = 1;
                    break;
                case GLFW_KEY_LEFT_CONTROL:
                    velocity.y = -1;
                    break;
                default:
                    break;
            }
        }

        if (action == GLFW_RELEASE) {
            switch (key) {
                case GLFW_KEY_W:
                    velocity.z = 0;
                    break;
                case GLFW_KEY_S:
                    velocity.z = 0;
                    break;
                case GLFW_KEY_A:
                    velocity.x = 0;
                    break;
                case GLFW_KEY_D:
                    velocity.x = 0;
                    break;
                case GLFW_KEY_SPACE:
                    velocity.y = 0;
                    break;
                case GLFW_KEY_LEFT_CONTROL:
                    velocity.y = 0;
                    break;
                default:
                    break;
            }
        }
    }


    void Camera::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
        Camera *camera = reinterpret_cast<Camera *>(glfwGetWindowUserPointer(window));
        camera->processKeypress(key, action);
    }

    void Camera::processMouseButton(int button, int action) {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
            shouldRotateCamera = true;
        }

        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
            shouldRotateCamera = false;
        }
    }

    void Camera::mouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
        Camera *camera = reinterpret_cast<Camera *>(glfwGetWindowUserPointer(window));
        camera->processMouseButton(button, action);
    }
}