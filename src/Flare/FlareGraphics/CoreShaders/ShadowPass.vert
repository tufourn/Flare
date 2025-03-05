#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#include "CoreShaders/BindlessCommon.glsl"

void main() {
    const uint indirectDrawDataBufferIndex = pc.data0;
    const uint positionBufferIndex = pc.data1;
    const uint transformBufferIndex = pc.data2;
    const uint lightBufferIndex = pc.data3;

    IndirectDrawData dd = indirectDrawDataAlias[indirectDrawDataBufferIndex].indirectDrawDatas[gl_DrawID];
    Light light = lightAlias[lightBufferIndex].light;

    mat4 transform = transformAlias[transformBufferIndex].transforms[dd.transformOffset];
    vec4 position = positionAlias[positionBufferIndex].positions[gl_VertexIndex];

    gl_Position = light.lightViewProjection * transform * position;
}