#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout (set = 0, binding = 0) buffer Dummy { vec4 v[]; } dummy[];
layout (set = 1, binding = 0) uniform texture2D globalTextures[];
layout (set = 2, binding = 0) uniform image2D globalStorageImages[];
layout (set = 3, binding = 0) uniform sampler globalSamplers[];

void main() {
}
