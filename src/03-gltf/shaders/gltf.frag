#version 460

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_samplerless_texture_functions : enable
#extension GL_GOOGLE_include_directive : enable

#include "CoreShaders/BindlessCommon.glsl"
#include "CoreShaders/SrgbToLinear.glsl"
#include "CoreShaders/CubemapCommon.glsl"

layout (location = 0) in vec3 inFragPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) flat in uint inDrawID;
layout (location = 3) in mat3 inTBN;
layout (location = 6) in vec4 inFragLightSpace;

layout (location = 0) out vec4 outColor;

struct Globals {
    mat4 mvp;

    vec4 cameraPos;

    uint positionBufferIndex;
    uint normalBufferIndex;
    uint uvBufferIndex;
    uint transformBufferIndex;

    uint textureBufferIndex;
    uint materialBufferIndex;
    uint indirectDrawDataBufferIndex;
    uint tangentBufferIndex;

    uint shadowDepthTextureIndex;
    uint shadowSamplerIndex;
    uint irradianceMapIndex;
    uint prefilteredCubeIndex;

    uint brdfLutIndex;
    uint cubemapSamplerIndex;
};

struct PbrInfo {
    float NoL;
    float NoV;
    float NoH;
    float LoH;
    float VoH;
    float perceptualRoughness;
    float metalness;
    vec3 reflectance0;
    vec3 reflectance90;
    float alphaRoughness;
    vec3 diffuseColor;
    vec3 specularColor;
};

layout (set = 0, binding = 0) uniform U { Globals globals; } globalBuffer[];

vec3 diffuse(PbrInfo pbrInfo) {
    return pbrInfo.diffuseColor / PI;
}

vec3 specularReflection(PbrInfo pbrInfo) {
    return pbrInfo.reflectance0 + (pbrInfo.reflectance90 - pbrInfo.reflectance0) * pow(clamp(1.0 - pbrInfo.VoH, 0.0, 1.0), 5.0);
}

float geometricOcclusion(PbrInfo pbrInfo)
{
    float NoL = pbrInfo.NoL;
    float NoV = pbrInfo.NoV;
    float r = pbrInfo.alphaRoughness;

    float attenuationL = 2.0 * NoL / (NoL + sqrt(r * r + (1.0 - r * r) * (NoL * NoL)));
    float attenuationV = 2.0 * NoV / (NoV + sqrt(r * r + (1.0 - r * r) * (NoV * NoV)));
    return attenuationL * attenuationV;
}

float microfacetDistribution(PbrInfo pbrInfo) {
    float roughnessSq = pbrInfo.alphaRoughness * pbrInfo.alphaRoughness;
    float f = (pbrInfo.NoH * roughnessSq - pbrInfo.NoH) * pbrInfo.NoH + 1.0;
    return roughnessSq / (PI * f * f);
}

float shadowCalculation(vec4 fragPosLightSpace, vec2 off, uint shadowDepthTextureIndex, uint shadowSamplerIndex) {
    float shadow = 1.0;
    if (fragPosLightSpace.z > -1.0 && fragPosLightSpace.z < 1.0) {
        float dist = texture(sampler2D(globalTextures[nonuniformEXT(shadowDepthTextureIndex)],
        globalSamplers[shadowSamplerIndex]), fragPosLightSpace.xy + off).r;

        if (fragPosLightSpace.w > 0.0 && dist < fragPosLightSpace.z) {
            shadow = 0.1;
        }
    }
    return shadow;
}

float lodFromRoughness(float perceptualRoughness) {
    const float MAX_LOD = 10.0;// cube res is 1024
    return perceptualRoughness * MAX_LOD;
}

vec3 getIblContribution(PbrInfo pbrInfo, vec3 N, vec3 R) {
    Globals glob = globalBuffer[pc.uniformOffset].globals;

    float lod = lodFromRoughness(pbrInfo.perceptualRoughness);
    vec3 brdf = GET_TEXTURE(glob.brdfLutIndex, glob.cubemapSamplerIndex, vec2(pbrInfo.NoV, 1.0 - pbrInfo.perceptualRoughness)).rgb;

    vec3 diffuseLight = srgbToLinear(GET_CUBEMAP(glob.irradianceMapIndex, glob.cubemapSamplerIndex, N)).rgb;
    vec3 specularLight = srgbToLinear(GET_CUBEMAP_LOD(glob.prefilteredCubeIndex, glob.cubemapSamplerIndex, R, lod)).rgb;

    vec3 diffuse = diffuseLight * pbrInfo.diffuseColor;
    vec3 specular = specularLight * (pbrInfo.specularColor * brdf.x + brdf.y);

    return diffuse + specular;
}

float filterPCF(vec4 fragPosLightSpace, uint shadowDepthTextureIndex, uint shadowSamplerIndex) {
    ivec2 texDim = textureSize(globalTextures[nonuniformEXT(shadowDepthTextureIndex)], 0);

    float scale = 1.5;
    float dx = scale * 1.0 / float(texDim.x);
    float dy = scale * 1.0 / float(texDim.y);

    float shadowFactor = 0.0;
    int count = 0;
    int range = 1;

    for (int x = -range; x <= range; x++) {
        for (int y = -range; y <= range; y++) {
            shadowFactor += shadowCalculation(fragPosLightSpace, vec2(dx*x, dy*y), shadowDepthTextureIndex, shadowSamplerIndex);
            count++;
        }
    }

    return shadowFactor / count;
}

void main() {
    uint lightBufferIndex = pc.data0;

    Globals glob = globalBuffer[pc.uniformOffset].globals;
    IndirectDrawData dd = indirectDrawDataAlias[glob.indirectDrawDataBufferIndex].indirectDrawDatas[inDrawID];

    Light light = lightAlias[lightBufferIndex].light;

    vec3 lightPos = light.lightPos.xyz;

    Material mat = materialAlias[glob.materialBufferIndex].materials[dd.materialOffset];

    TextureIndex albedoTex = textureIndexAlias[glob.textureBufferIndex].textureIndices[mat.albedoTextureOffset];
    vec4 albedo = srgbToLinear(GET_TEXTURE(albedoTex.textureIndex, albedoTex.samplerIndex, inUV)) * mat.albedoFactor;

    if (albedo.a < 0.5) {
        discard;
    }

    TextureIndex normalTex = textureIndexAlias[glob.textureBufferIndex].textureIndices[mat.normalTextureOffset];
    vec3 normal = GET_TEXTURE(normalTex.textureIndex, normalTex.samplerIndex, inUV).rgb;

    TextureIndex metallicRoughnessTex = textureIndexAlias[glob.textureBufferIndex].textureIndices[mat.metallicRoughnessTextureOffset];
    vec4 metallicRoughness = GET_TEXTURE(metallicRoughnessTex.textureIndex, metallicRoughnessTex.samplerIndex, inUV);

    vec3 emissive = mat.emissiveFactor;
    TextureIndex emissiveTex = textureIndexAlias[glob.textureBufferIndex].textureIndices[mat.emissiveTextureOffset];
    emissive *= srgbToLinear(GET_TEXTURE(emissiveTex.textureIndex, emissiveTex.samplerIndex, inUV)).rgb;

    TextureIndex aoTex = textureIndexAlias[glob.textureBufferIndex].textureIndices[mat.occlusionTextureOffset];
    float ao = GET_TEXTURE(aoTex.textureIndex, aoTex.samplerIndex, inUV).r;

    // green channel for roughness, clamped to 0.089 to avoid division by 0
    float perceptualRoughness = clamp(mat.roughnessFactor * metallicRoughness.g, 0.089, 1.0);
    float alphaRoughness = perceptualRoughness * perceptualRoughness;

    // blue channel for metallic
    float metalness = mat.metallicFactor * metallicRoughness.b;

    vec3 f0 = vec3(0.04);

    vec3 diffuseColor = (1.0 - metalness) * (albedo.rgb * (vec3(1.0) - f0));
    vec3 specularColor = mix(f0, albedo.rgb, metalness);

    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
    vec3 specularEnvironmentR0 = specularColor.rgb;
    vec3 specularEnvironmentR90 = vec3(1.0) * reflectance90;

    vec3 N = normalize(inTBN * (normal * 2.0 - 1.0));
    vec3 L = normalize(lightPos - inFragPos);
    vec3 V = normalize(glob.cameraPos.xyz - inFragPos);
    vec3 H = normalize(L + V);

    vec3 reflection = normalize(reflect(-V, N));
    reflection.y *= -1;

    float NoV = clamp(abs(dot(N, V)), 0.001, 1.0);
    float NoL = clamp(dot(N, L), 0.001, 1.0);
    float NoH = clamp(dot(N, H), 0.0, 1.0);
    float LoH = clamp(dot(L, H), 0.0, 1.0);
    float VoH = clamp(dot(V, H), 0.0, 1.0);

    PbrInfo pbrInfo = PbrInfo(
    NoL,
    NoV,
    NoH,
    LoH,
    VoH,
    perceptualRoughness,
    metalness,
    specularEnvironmentR0,
    specularEnvironmentR90,
    alphaRoughness,
    diffuseColor,
    specularColor
    );

    vec3 F = specularReflection(pbrInfo);
    float G = geometricOcclusion(pbrInfo);
    float D = microfacetDistribution(pbrInfo);

    vec3 diffuseContrib = (1.0 - F) * diffuse(pbrInfo);
    vec3 specularContrib = F * G * D / (4.0 * NoL * NoV);

    float shadow = filterPCF(inFragLightSpace, glob.shadowDepthTextureIndex, glob.shadowSamplerIndex);
//    vec3 color = NoL * lightIntensity * lightColor * (diffuseContrib + specularContrib) * shadow;
//    vec3 color = NoL * (diffuseContrib + specularContrib) * shadow;
    vec3 color = NoL * (diffuseContrib);

//    color += getIblContribution(pbrInfo, N, reflection);
//
//    const float occlusionStrength = 1.0;
//    color = mix(color, color * ao, occlusionStrength);
//
//    color += emissive;

    outColor = vec4(color, 1.0);
}
