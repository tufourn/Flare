#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#include "CoreShaders/BindlessCommon.glsl"

struct ShadowUniform {
    mat4 lightSpaceMatrix;

    uint drawDataBufferIndex;
    uint positionBufferIndex;
    uint transformBufferIndex;
    float pad;
};

layout (set = 0, binding = 0) uniform U { ShadowUniform shadowUniform; } shadowUniformAlias[];

void main() {
    ShadowUniform shadowUniform = shadowUniformAlias[pc.uniformOffset].shadowUniform;
    DrawData dd = drawDataAlias[shadowUniform.drawDataBufferIndex].drawDatas[gl_DrawID];

    mat4 transform = transformAlias[shadowUniform.transformBufferIndex].transforms[dd.transformOffset];
    vec4 position = positionAlias[shadowUniform.positionBufferIndex].positions[gl_VertexIndex];

    gl_Position = shadowUniform.lightSpaceMatrix * transform * position;
}