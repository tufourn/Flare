#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout (push_constant) uniform PushConstants {
    uint globalIndex;
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
    uint tangentBufferIndex;

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

layout (set = 1, binding = 0) readonly buffer Tangent {
    vec4 tangents[];
} tangentBuffer[];

layout (location = 0) out vec3 outFragPos;
layout (location = 1) out vec2 outUV;
layout (location = 2) out uint outDrawID;
layout (location = 3) out mat3 outTBN;

void main() {
    Globals glob = globalBuffer[pc.globalIndex].globals;

    GpuMeshDraw md = meshDrawBuffer[glob.meshDrawBufferIndex].meshDraws[gl_DrawID];

    mat4 transform = transformBuffer[glob.transformBufferIndex].transforms[md.transformOffset];

    vec4 position = transform * positionBuffer[glob.positionBufferIndex].positions[gl_VertexIndex];
    gl_Position = glob.mvp * position;

    vec3 normal = normalize(mat3(transpose(inverse(transform))) * normalBuffer[glob.normalBufferIndex].normals[gl_VertexIndex].xyz);
    vec4 tangent = normalize(transform * tangentBuffer[glob.tangentBufferIndex].tangents[gl_VertexIndex]);
    vec3 bitangent = normalize(cross(normal, tangent.xyz) * tangent.w);

    outTBN = mat3(tangent.xyz, bitangent, normal);

    outFragPos = position.xyz;
    outUV = uvBuffer[glob.uvBufferIndex].uvs[gl_VertexIndex];
    outDrawID = gl_DrawID;
}
