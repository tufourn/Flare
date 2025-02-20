#version 450

#extension GL_EXT_nonuniform_qualifier : enable

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

layout (set = 1, binding = 0) readonly buffer Transform {
    mat4 transforms[];
} transformBuffer[];

layout (set = 1, binding = 0) readonly buffer Position {
    vec4 positions[];
} positionBuffer[];

layout (location = 0) out vec2 uvOut;

void main() {
    Globals glob = globalBuffer[pc.globalIndex].globals;

    GpuMeshDraw md = meshDrawBuffer[glob.meshDrawBufferIndex].meshDraws[pc.meshDrawIndex];

    mat4 transform = transformBuffer[glob.transformBufferIndex].transforms[md.transformOffset];

    vec2 uv = uvBuffer[glob.uvBufferIndex].uvs[gl_VertexIndex];
    vec4 position = positionBuffer[glob.positionBufferIndex].positions[gl_VertexIndex];

    gl_Position = glob.mvp * transform * position;
    uvOut = uv;
}
