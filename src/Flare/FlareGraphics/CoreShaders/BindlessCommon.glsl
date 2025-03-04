#ifndef SHADER_BINDLESS_COMMON_GLSL
#define SHADER_BINDLESS_COMMON_GLSL

// Push constant containing binding offset of an uniform buffer which can contain the offsets to the other SBOs
layout (push_constant) uniform PushConstants {
    mat4 mat;

    uint uniformOffset;
    uint data0;
    uint data1;
    uint data2;

    uint data3;
    uint data4;
    uint data5;
uint data6;
} pc;

// Textures and samplers
layout (set = 2, binding = 0) uniform texture2D globalTextures[];
layout (set = 3, binding = 0) uniform writeonly image2D globalStorageImages[];
layout (set = 4, binding = 0) uniform sampler globalSamplers[];

#define GET_TEXTURE(textureIndex, samplerIndex, uv) \
texture(sampler2D(globalTextures[textureIndex], globalSamplers[samplerIndex]), uv)

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

    vec3 emissiveFactor;
    uint emissiveTextureOffset;

    uint albedoTextureOffset;
    uint metallicRoughnessTextureOffset;
    uint normalTextureOffset;
    uint occlusionTextureOffset;

    float metallicFactor;
    float roughnessFactor;
    float pad[2];
};
layout(set = 1, binding = 0) readonly buffer MaterialBuffer {
    Material materials[];
} materialAlias[];

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

layout(set = 1, binding = 0) readonly buffer IndirectDrawDataBuffer {
    IndirectDrawData indirectDrawDatas[];
} indirectDrawDataAlias[];

struct Bounds {
    vec3 origin;
    float radius;
    vec3 extents;
    float pad;
};

const float PI = 3.141592653589;

#endif
