#pragma once

#include <filesystem>
#include <cgltf.h>
#include "GpuDevice.h"

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/quaternion.hpp>

namespace Flare {
    struct AsyncLoader;

    struct PBRMaterial {
        glm::vec4 baseColorFactor;

        int baseTextureOffset = -1;
        int metallicRoughnessTextureOffset = -1;
        int normalTextureOffset = -1;
        int occlusionTextureOffset = -1;
        int emissiveTextureOffset = -1;

        float metallicFactor = 1.f;
        float roughnessFactor = 1.f;

    };

    struct MeshPrimitive {
        Handle<Buffer> indexBufferHandle;
        int indexOffset = -1;

        Handle<Buffer> positionBufferHandle;
        int positionOffset = -1;

        Handle<Buffer> normalBufferHandle;
        int normalOffset = -1;

        Handle<Buffer> texcoordBufferHandle;
        int texcoordOffset = -1;

        uint32_t vertexCount = 0;
    };

    struct Mesh {
        std::vector<MeshPrimitive> meshPrimitives;
    };

    struct Node {
        Node *parent = nullptr;
        std::vector<Node *> children;

        Mesh *mesh = nullptr;

        glm::mat4 matrix = glm::mat4(1.f);
        glm::vec3 translation;
        glm::quat rotation;
        glm::vec3 scale;

        glm::mat4 getLocalTransform() const;
    };

    struct GltfScene {
        std::vector<Handle<Buffer>> buffers;
        std::vector<Handle<Texture>> textures;
        std::vector<Handle<Sampler>> samplers;

        std::vector<Node> nodes;
        std::vector<const Node *> topLevelNodes;
        std::vector<Mesh> meshes;
        std::vector<PBRMaterial> materials;

        GpuDevice *gpu = nullptr;
        cgltf_data *data = nullptr;

        void init(const std::filesystem::path &path, GpuDevice *gpuDevice, AsyncLoader *asyncLoader);

        void shutdown();

        void prepareDraws();
    };
}
