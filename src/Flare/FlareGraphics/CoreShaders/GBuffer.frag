#version 460

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#include "CoreShaders/BindlessCommon.glsl"
#include "CoreShaders/SrgbToLinear.glsl"
#include "CoreShaders/NormalEncoding.glsl"

layout (location = 0) in vec2 inUV;
layout (location = 1) in flat uint inDrawID;
layout (location = 2) in vec3 inModelSpacePos;
layout (location = 3) in vec4 inClipSpacePos;
layout (location = 4) in vec4 inPrevClipSpacePos;
layout (location = 5) in mat3 inTBN;

layout (location = 0) out vec4 outAlbedo;
layout (location = 1) out vec2 outNormal;

void main() {
    uint indirectDrawIndex = pc.data0;
    uint transformIndex = pc.data1;
    uint materialIndex = pc.data2;
    uint textureIndex = pc.data3;

    IndirectDrawData dd = indirectDrawDataAlias[indirectDrawIndex].indirectDrawDatas[inDrawID];
    Material mat = materialAlias[materialIndex].materials[dd.materialOffset];

    TextureIndex albedoIndex = textureIndexAlias[textureIndex].textureIndices[mat.albedoTextureOffset];
    outAlbedo = GET_TEXTURE(albedoIndex.textureIndex, albedoIndex.samplerIndex, inUV);

    TextureIndex normalIndex = textureIndexAlias[textureIndex].textureIndices[mat.normalTextureOffset];
    vec3 normal = GET_TEXTURE(normalIndex.textureIndex, normalIndex.samplerIndex, inUV).rgb;
    outNormal = octahedralEncode(normalize(inTBN * (normal * 2.0 - 1.0)));
}
