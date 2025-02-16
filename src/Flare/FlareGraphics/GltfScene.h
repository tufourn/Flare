#pragma once

#include <filesystem>
#include <cgltf.h>
#include "GpuDevice.h"

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/quaternion.hpp>

namespace Flare {
    struct AsyncLoader;

    struct GltfTexture {
        Handle<Texture> image;
        Handle<Sampler> sampler;
    };

    struct MeshDraw {
        uint32_t indexCount = 0;
        int32_t indexOffset = -1;
        int32_t vertexOffset = -1;
        int32_t transformOffset = -1;
    };

    struct SkinData { // todo
        int32_t positionOffset = -1;
    };

    struct PBRMaterial {
        glm::vec4 baseColorFactor;

        int32_t baseTextureOffset = -1;
        int32_t metallicRoughnessTextureOffset = -1;
        int32_t normalTextureOffset = -1;
        int32_t occlusionTextureOffset = -1;
        int32_t emissiveTextureOffset = -1;

        float metallicFactor = 1.f;
        float roughnessFactor = 1.f;
    };

    struct GltfMeshPrimitive {
        uint32_t id;
        std::vector<glm::vec4> positions;
        std::vector<uint32_t> indices;
        std::vector<glm::vec4> normals;
        std::vector<glm::vec2> uvs;
    };

    struct GltfMesh {
        std::vector<GltfMeshPrimitive> meshPrimitives;
    };

    struct Node {
        Node *parent = nullptr;
        std::vector<Node *> children;

        GltfMesh *mesh = nullptr;

        glm::mat4 worldTransform = glm::mat4(1.f);

        glm::mat4 matrix = glm::mat4(1.f);
        glm::vec3 translation;
        glm::quat rotation;
        glm::vec3 scale;

        int skin = -1; //todo

        glm::mat4 getLocalTransform() const;

        void updateWorldTransform();
    };

    struct GltfScene {
        std::vector<Handle<Texture>> textures;
        std::vector<Handle<Sampler>> samplers;
        std::vector<GltfTexture> gltfTextures;

        std::vector<Node> nodes;
        std::vector<const Node *> topLevelNodes;
        std::vector<GltfMesh> meshes;
        std::vector<PBRMaterial> materials;

        std::vector<glm::vec4> positions;
        std::vector<uint32_t> indices;
        std::vector<glm::vec2> uvs;
        std::vector<glm::vec4> normals;
        std::vector<glm::mat4> transforms;

        std::vector<MeshDraw> meshDraws;

        GpuDevice *gpu = nullptr;
        cgltf_data *data = nullptr;

        void init(const std::filesystem::path &path, GpuDevice *gpuDevice, AsyncLoader *asyncLoader);

        void shutdown();

        void generateMeshDrawsFromNode(Node *node, std::unordered_map<uint32_t, std::vector<MeshDraw>> &map);
    };
}
