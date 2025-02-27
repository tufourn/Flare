#version 460

#extension GL_GOOGLE_include_directive : enable

#include "CoreShaders/BindlessCommon.glsl"

layout (location = 0) in vec4 position;

layout (location = 0) out vec3 outUVW;

void main() {
    outUVW = position.xyz;
    outUVW.y *= -1.0;

    gl_Position = (pc.mat * position).xyww;
}
