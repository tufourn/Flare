#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#include "CoreShaders/BindlessCommon.glsl"

struct ShadowUniform {
    mat4 lightSpaceMatrix;

    uint indirectDrawDataBufferIndex;
    uint positionBufferIndex;
    uint transformBufferIndex;
    float pad;
};

layout (set = 0, binding = 0) uniform U { ShadowUniform shadowUniform; } shadowUniformAlias[];

void main() {
    ShadowUniform shadowUniform = shadowUniformAlias[pc.uniformOffset].shadowUniform;
    IndirectDrawData dd = indirectDrawDataAlias[shadowUniform.indirectDrawDataBufferIndex].indirectDrawDatas[gl_DrawID];

    mat4 transform = transformAlias[shadowUniform.transformBufferIndex].transforms[dd.transformOffset];
    vec4 position = positionAlias[shadowUniform.positionBufferIndex].positions[gl_VertexIndex];

    gl_Position = shadowUniform.lightSpaceMatrix * transform * position;
}