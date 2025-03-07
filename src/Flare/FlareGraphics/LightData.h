#pragma once

#include <glm/glm.hpp>

namespace Flare {
struct LightData {
  void setPos(glm::vec3 pos);

  void setDir(glm::vec3 dir);

  void setColor(glm::vec3 color);

  void updateViewProjection();

  glm::mat4 getViewProjection() const;

  glm::mat4 lightViewProjection = glm::mat4(1.f);
  glm::vec4 lightPos = {-4.f, 12.f, 2.f, 1.f};
  glm::vec4 lightDir = {2.f, 10.f, 0.f, 0.f};
  glm::vec4 lightColor = {1.f, 1.f, 1.f, 1.f};
};
} // namespace Flare
