#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#include "CoreShaders/BindlessCommon.glsl"

struct ShadowUniform {
    mat4 lightSpaceMatrix;
};

layout (set = 0, binding = 0) uniform U { ShadowUniform shadowUniform; } shadowUniformAlias[];

void main() {
    const uint indirectDrawDataBufferIndex = pc.data0;
    const uint positionBufferIndex = pc.data1;
    const uint transformBufferIndex = pc.data2;
    ShadowUniform shadowUniform = shadowUniformAlias[pc.uniformOffset].shadowUniform;
    IndirectDrawData dd = indirectDrawDataAlias[indirectDrawDataBufferIndex].indirectDrawDatas[gl_DrawID];

    mat4 transform = transformAlias[transformBufferIndex].transforms[dd.transformOffset];
    vec4 position = positionAlias[positionBufferIndex].positions[gl_VertexIndex];

    gl_Position = shadowUniform.lightSpaceMatrix * transform * position;
}