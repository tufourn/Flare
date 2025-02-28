#version 460

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#include "CoreShaders/BindlessCommon.glsl"
#include "CoreShaders/SrgbToLinear.glsl"
#include "CoreShaders/CubemapCommon.glsl"

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 outColor;

void main() {
    outColor = srgbToLinear(GET_CUBEMAP(pc.data0, pc.data1, inUVW));
}
