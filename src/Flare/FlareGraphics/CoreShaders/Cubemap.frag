#version 460

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#include "CoreShaders/BindlessCommon.glsl"

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 outColor;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 sampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main() {
    vec2 uv = sampleSphericalMap(normalize(inUVW));
    vec3 color = texture(sampler2D(globalTextures[pc.data0], globalSamplers[pc.data1]), uv).rgb;

    outColor = vec4(color, 1.0);
}
