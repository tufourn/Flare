#version 460

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#include "CoreShaders/BindlessCommon.glsl"
#include "CoreShaders/SrgbToLinear.glsl"

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 outColor;

void main() {
    outColor = srgbToLinear(texture(samplerCube(cubemapTextures[pc.data0], globalSamplers[pc.data1]), inUVW)).xyzw;
}
