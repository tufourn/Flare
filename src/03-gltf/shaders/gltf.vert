#version 460

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#include "CoreShaders/BindlessCommon.glsl"

struct Light {
    vec3 position;
    float radius;
    vec3 color;
    float intensity;
};

struct Globals {
    mat4 mvp;
    mat4 lightSpaceMatrix;

    uint positionBufferIndex;
    uint normalBufferIndex;
    uint uvBufferIndex;
    uint transformBufferIndex;

    uint textureBufferIndex;
    uint materialBufferIndex;
    uint meshDrawBufferIndex;
    uint tangentBufferIndex;

    Light light;

    uint shadowDepthTextureIndex;
    uint shadowSamplerIndex;
    float pad[2];
};

const mat4 biasMat = mat4(
0.5, 0.0, 0.0, 0.0,
0.0, 0.5, 0.0, 0.0,
0.0, 0.0, 1.0, 0.0,
0.5, 0.5, 0.0, 1.0
);

layout (set = 0, binding = 0) uniform U { Globals globals; } globalBuffer[];

layout (location = 0) out vec3 outFragPos;
layout (location = 1) out vec2 outUV;
layout (location = 2) out uint outDrawID;
layout (location = 3) out mat3 outTBN;
layout (location = 6) out vec4 outFragLightSpace;

void main() {
    Globals glob = globalBuffer[pc.uniformOffset].globals;

    DrawData dd = drawDataAlias[glob.meshDrawBufferIndex].drawDatas[gl_DrawID];

    mat4 transform = transformAlias[glob.transformBufferIndex].transforms[dd.transformOffset];

    vec4 position = transform * positionAlias[glob.positionBufferIndex].positions[gl_VertexIndex];
    gl_Position = glob.mvp * position;

    vec3 normal = normalize(mat3(transpose(inverse(transform))) * normalAlias[glob.normalBufferIndex].normals[gl_VertexIndex].xyz);
    vec4 tangent = normalize(transform * tangentAlias[glob.tangentBufferIndex].tangents[gl_VertexIndex]);
    vec3 bitangent = normalize(cross(normal, tangent.xyz) * tangent.w);

    outTBN = mat3(tangent.xyz, bitangent, normal);

    outFragPos = position.xyz;
    outUV = uvAlias[glob.uvBufferIndex].uvs[gl_VertexIndex];
    outDrawID = gl_DrawID;
    outFragLightSpace = biasMat * glob.lightSpaceMatrix * position;
}
