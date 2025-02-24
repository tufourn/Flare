#ifndef SHADER_BINDLESS_COMMON_GLSL
#define SHADER_BINDLESS_COMMON_GLSL

// Push constant containing binding offset of an uniform buffer which can contain the offsets to the other SBOs
layout (push_constant) uniform PushConstants {
    uint uniformOffset;
    // we can add other stuff here as well
} pc;

// Textures and samplers
layout (set = 2, binding = 0) uniform texture2D globalTextures[];
layout (set = 3, binding = 0) uniform writeonly image2D globalStorageImages[];
layout (set = 4, binding = 0) uniform sampler globalSamplers[];

// Aliased SSBOs
layout (set = 1, binding = 0) readonly buffer PositionBuffer {
    vec4 positions[];
} positionAlias[];

layout (set = 1, binding = 0) readonly buffer NormalBuffer {
    vec4 normals[];
} normalAlias[];

layout (set = 1, binding = 0) readonly buffer TangentBuffer {
    vec4 tangents[];
} tangentAlias[];

layout (set = 1, binding = 0) readonly buffer UvBuffer {
    vec2 uvs[];
} uvAlias[];

layout (set = 1, binding = 0) readonly buffer TransformBuffer {
    mat4 transforms[];
} transformAlias[];

struct TextureIndex {
    uint textureIndex;
    uint samplerIndex;
};
layout(set = 1, binding = 0) readonly buffer TextureIndexBuffer {
    TextureIndex textureIndices[];
} textureIndexAlias[];

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
layout(set = 1, binding = 0) readonly buffer MaterialBuffer {
    Material materials[];
} materialAlias[];

layout(set = 1, binding = 0) buffer indirectCountBuffer {
    uint count;
} indirectCountAlias[];

struct IndirectDrawData {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    uint vertexOffset;
    uint firstInstance;

    uint meshId;
    uint materialOffset;
    uint transformOffset;
};
layout(set = 1, binding = 0) buffer indirectDrawDataBuffer {
    IndirectDrawData indirectDrawDatas[];
} indirectDrawDataAlias[];

#endif
