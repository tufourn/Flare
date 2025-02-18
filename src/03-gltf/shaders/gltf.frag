#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout (set = 1, binding = 0) uniform texture2D globalTextures[];
layout (set = 2, binding = 0) uniform writeonly image2D globalStorageImages[];
layout (set = 3, binding = 0) uniform sampler globalSamplers[];

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

void main() {
    outColor = texture(sampler2D(globalTextures[4], globalSamplers[0]), inUV);
}
