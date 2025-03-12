#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#include "CoreShaders/BindlessCommon.glsl"

layout (location = 0) in vec4 position;

layout (set = 1, binding = 0) readonly buffer BoundsBuffer {
    Bounds bounds[];
} boundsAlias[];

void main() {
    mat4 viewProjection = pc.mat;
    uint boundBufferIndex = pc.data0;
    uint transformBufferIndex = pc.data1;
    uint index = pc.data2;

    mat4 transform = transformAlias[transformBufferIndex].transforms[index];
    Bounds bounds = boundsAlias[boundBufferIndex].bounds[index];

    gl_Position = viewProjection * transform * vec4(bounds.origin + vec3(position) * bounds.extents, 1.0);
}