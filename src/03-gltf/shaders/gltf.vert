#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout (push_constant) uniform PushConstants {
    uint globalIndex;
    uint meshDrawIndex;
} pc;

struct Light {
    vec3 position;
    float radius;
    vec3 color;
    float intensity;
};

struct Globals {
    mat4 mvp;

    uint positionBufferIndex;
    uint normalBufferIndex;
    uint uvBufferIndex;
    uint transformBufferIndex;

    uint textureBufferIndex;
    uint materialBufferIndex;
    uint meshDrawBufferIndex;
    float pad;

    Light light;
};

layout (set = 0, binding = 0) uniform U { Globals globals; } globalBuffer[];

struct GpuMeshDraw {
    uint transformOffset;
    uint materialOffset;
};

// aliasing ssbo
layout (set = 1, binding = 0) readonly buffer MeshDraws {
    GpuMeshDraw meshDraws[];
} meshDrawBuffer[];

layout (set = 1, binding = 0) readonly buffer UVs {
    vec2 uvs[];
} uvBuffer[];

layout (set = 1, binding = 0) readonly buffer Normals {
    vec4 normals[];
} normalBuffer[];

layout (set = 1, binding = 0) readonly buffer Transform {
    mat4 transforms[];
} transformBuffer[];

layout (set = 1, binding = 0) readonly buffer Position {
    vec4 positions[];
} positionBuffer[];

layout (location = 0) out vec3 outFragPos;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outNormal;

void main() {
    Globals glob = globalBuffer[pc.globalIndex].globals;

    GpuMeshDraw md = meshDrawBuffer[glob.meshDrawBufferIndex].meshDraws[pc.meshDrawIndex];

    mat4 transform = transformBuffer[glob.transformBufferIndex].transforms[md.transformOffset];

    vec4 position = transform * positionBuffer[glob.positionBufferIndex].positions[gl_VertexIndex];
    gl_Position = glob.mvp * position;

    outFragPos = position.xyz;
    outUV = uvBuffer[glob.uvBufferIndex].uvs[gl_VertexIndex];
    outNormal = mat3(transpose(inverse(transform))) * normalBuffer[glob.normalBufferIndex].normals[gl_VertexIndex].xyz;
}
