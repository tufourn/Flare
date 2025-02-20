#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout (set = 2, binding = 0) uniform texture2D globalTextures[];
layout (set = 3, binding = 0) uniform writeonly image2D globalStorageImages[];
layout (set = 4, binding = 0) uniform sampler globalSamplers[];

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

layout (push_constant) uniform PushConstants {
    uint globalIndex;
    uint meshDrawIndex;
} pc;

struct Globals {
    mat4 mvp;

    uint positionBufferIndex;
    uint uvBufferIndex;
    uint transformBufferIndex;
    uint textureBufferIndex;

    uint materialBufferIndex;
    uint meshDrawBufferIndex;
    float pad[2];
};

struct Material {
    vec4 albedoFactor;

    uint albedoTextureOffset;
    uint metallicRoughnessTextureOffset;
    uint normalTextureOffset;
    uint occlusionTextureOffset;

    uint emissiveTextureOffset;
    float metallicFactor;
    float roughnessFactor;
    float pad;
};

layout (set = 0, binding = 0) uniform U { Globals globals; } globalBuffer[];

struct GpuMeshDraw {
    uint transformOffset;
    uint materialOffset;
};

struct TextureIndex {
    uint textureIndex;
    uint samplerIndex;
};

// aliasing ssbo
layout (set = 1, binding = 0) readonly buffer MeshDraws {
    GpuMeshDraw meshDraws[];
} meshDrawBuffer[];

layout (set = 1, binding = 0) readonly buffer T {
    TextureIndex textureIndices[];
} textureIndexBuffer[];

layout (set = 1, binding = 0) readonly buffer M {
    Material materials[];
} materialBuffer[];

void main() {
    Globals glob = globalBuffer[pc.globalIndex].globals;
    GpuMeshDraw md = meshDrawBuffer[glob.meshDrawBufferIndex].meshDraws[pc.meshDrawIndex];

    Material mat = materialBuffer[glob.materialBufferIndex].materials[md.materialOffset];

    TextureIndex tex = textureIndexBuffer[glob.textureBufferIndex].textureIndices[mat.albedoTextureOffset];

    outColor = texture(sampler2D(globalTextures[nonuniformEXT(tex.textureIndex)], globalSamplers[nonuniformEXT(tex.samplerIndex)]), inUV);
}
