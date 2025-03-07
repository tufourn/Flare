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
layout (location = 2) out vec4 outOcclusionMetallicRoughness;
layout (location = 3) out vec4 outEmissive;

void main() {
    uint indirectDrawIndex = pc.data0;
    uint transformIndex = pc.data1;
    uint materialIndex = pc.data2;
    uint textureIndex = pc.data3;

    IndirectDrawData dd = indirectDrawDataAlias[indirectDrawIndex].indirectDrawDatas[inDrawID];
    Material mat = materialAlias[materialIndex].materials[dd.materialOffset];

    TextureIndex albedoIndex = textureIndexAlias[textureIndex].textureIndices[mat.albedoTextureOffset];
    vec4 albedo = srgbToLinear(GET_TEXTURE(albedoIndex.textureIndex, albedoIndex.samplerIndex, inUV)) * mat.albedoFactor;

    if (albedo.a < mat.alphaCutoff) {
        discard;
    }

    outAlbedo = albedo;

    TextureIndex normalIndex = textureIndexAlias[textureIndex].textureIndices[mat.normalTextureOffset];
    vec3 normal = GET_TEXTURE(normalIndex.textureIndex, normalIndex.samplerIndex, inUV).rgb;
    outNormal = octahedralEncode(normalize(inTBN * (normal * 2.0 - 1.0)));

    TextureIndex metallicRoughnessIndex = textureIndexAlias[textureIndex].textureIndices[mat.metallicRoughnessTextureOffset];
    vec4 metallicRoughness = GET_TEXTURE(metallicRoughnessIndex.textureIndex, metallicRoughnessIndex.samplerIndex, inUV);
    TextureIndex occlusionIndex = textureIndexAlias[textureIndex].textureIndices[mat.occlusionTextureOffset];
    float occlusion = GET_TEXTURE(occlusionIndex.textureIndex, occlusionIndex.samplerIndex, inUV).r;

    outOcclusionMetallicRoughness.r = occlusion;// occlusion
    outOcclusionMetallicRoughness.g = mat.metallicFactor * metallicRoughness.b;// metallic
    outOcclusionMetallicRoughness.b = clamp(mat.roughnessFactor * metallicRoughness.g, 0.089, 1.0);// roughness

    vec3 emissive = mat.emissiveFactor;
    TextureIndex emissiveIndex = textureIndexAlias[textureIndex].textureIndices[mat.emissiveTextureOffset];
    emissive *= srgbToLinear(GET_TEXTURE(emissiveIndex.textureIndex, emissiveIndex.samplerIndex, inUV)).rgb;
    outEmissive = vec4(emissive, 1.0);
}
