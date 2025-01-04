#include "Application.h"

namespace Flare {
    void Application::run(const ApplicationConfig &appConfig) {
        init(appConfig);
        loop();
        shutdown();
    }
}
