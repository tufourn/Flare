#ifndef SHADER_CUBEMAP_COMMON_GLSL
#define SHADER_CUBEMAP_COMMON_GLSL

layout (set = 2, binding = 0) uniform textureCube cubemapTextures[];
layout (set = 4, binding = 0) uniform sampler globalSamplers[];

#define GET_CUBEMAP(cubemapIndex, samplerIndex, uvw) \
texture(samplerCube(cubemapTextures[cubemapIndex], globalSamplers[samplerIndex]), uvw).xyzw

#define GET_CUBEMAP_LOD(cubemapIndex, samplerIndex, uvw, lod) \
textureLod(samplerCube(cubemapTextures[cubemapIndex], globalSamplers[samplerIndex]), uvw, lod).xyzw

#endif