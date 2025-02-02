#pragma once

#include <filesystem>
#include "GpuResources.h"
#include "Mesh.h"

namespace Flare {
    struct Scene {
        std::vector<MeshDraw> meshDraws;

        std::vector<Handle<Buffer>> buffers;
        std::vector<Handle<Texture>> textures;
        std::vector<Handle<Sampler>> samplers;
    };
}
