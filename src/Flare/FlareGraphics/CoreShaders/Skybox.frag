#version 460

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#include "CoreShaders/BindlessCommon.glsl"
#include "CoreShaders/SrgbToLinear.glsl"
#include "CoreShaders/CubemapCommon.glsl"

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 outColor;

void main() {
    const uint skyboxTexture = pc.data0;
    const uint skyboxSampler = pc.data1;

    outColor = srgbToLinear(GET_CUBEMAP(skyboxTexture, skyboxSampler, inUVW));
}
