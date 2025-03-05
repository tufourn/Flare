#include "LightData.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"

#include <glm/glm.hpp>

namespace Flare {
    void LightData::setPos(glm::vec3 pos) {
        lightPos.x = pos.x;
        lightPos.y = pos.y;
        lightPos.z = pos.z;
        updateViewProjection();
    }

    void LightData::setDir(glm::vec3 dir) {
        dir = glm::normalize(dir);
        lightDir.x = dir.x;
        lightDir.y = dir.y;
        lightDir.z = dir.z;
        updateViewProjection();
    }

    void LightData::updateViewProjection() {
        float fov = 45.f;
        float nearPlane = 1.f;
        float farPlane = 100.f;
        float aspectRatio = 1.f;

        glm::vec3 target = lightPos - lightDir;
        glm::mat4 view = glm::lookAt(glm::vec3(lightPos.x, lightPos.y, lightPos.z),
                                     glm::vec3(0.f, 0.f, 0.f),
                                     glm::vec3(0.f, 1.f, 0.f));
//        glm::mat4 projection = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
        glm::mat4 projection = glm::ortho(-10.f, 10.f, -10.f, 10.f, nearPlane, farPlane);
        projection[1][1] *= -1;

        lightViewProjection = projection * view;
    }

    void LightData::setColor(glm::vec3 color) {
        lightColor.r = color.r;
        lightColor.g = color.g;
        lightColor.b = color.b;
    }

    glm::mat4 LightData::getViewProjection() const {
        return lightViewProjection;
    }
}