#pragma once

#include <cstdint>

namespace Flare {
struct ApplicationConfig {
  ApplicationConfig &setWidth(uint32_t w) {
    width = w;
    return *this;
  }
  ApplicationConfig &setHeight(uint32_t h) {
    height = h;
    return *this;
  }
  ApplicationConfig &setName(const char *n) {
    name = n;
    return *this;
  }

  uint32_t width;
  uint32_t height;
  const char *name;
};

struct Application {
  virtual void init(const ApplicationConfig &appConfig) {};
  virtual void loop() {};
  virtual void shutdown() {};

  void run(const ApplicationConfig &appConfig);
};
} // namespace Flare
