#version 460

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_samplerless_texture_functions : enable
#extension GL_GOOGLE_include_directive : enable

#include "CoreShaders/BindlessCommon.glsl"

#define PI 3.14159265

layout (location = 0) in vec3 inFragPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) flat in uint inDrawID;
layout (location = 3) in mat3 inTBN;
layout (location = 6) in vec4 inFragLightSpace;

layout (location = 0) out vec4 outColor;

struct Light {
    vec3 position;
    float radius;
    vec3 color;
    float intensity;
};

struct Globals {
    mat4 mvp;
    mat4 lightSpaceMatrix;

    Light light;

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

layout (set = 0, binding = 0) uniform U { Globals globals; } globalBuffer[];

float D_GGX(float NoH, float a) {
    float a2 = a * a;
    float f = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * f * f);
}

vec3 F_Schlick(float u, vec3 f0) {
    return f0 + (vec3(1.0) - f0) * pow(1.0 - u, 5.0);
}

float V_SmithGGXCorrelated(float NoV, float NoL, float a) {
    float a2 = a * a;
    float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
    float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
    return 0.5 / (GGXV + GGXL);
}

float Fd_Lambert() {
    return 1.0 / PI;
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

float lodFromRoughness(float roughness) {
    const float MAX_LOD = 10.0;// cube res is 1024
    return roughness * MAX_LOD;
}

// learnopengl
vec3 fresnelSchlickRoughness(float NoV, vec3 f0, float roughness)
{
    return f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(clamp(1.0 - NoV, 0.0, 1.0), 5.0);
}

void main() {
    Globals glob = globalBuffer[pc.uniformOffset].globals;
    IndirectDrawData dd = indirectDrawDataAlias[glob.indirectDrawDataBufferIndex].indirectDrawDatas[inDrawID];

    vec3 lightPos = glob.light.position;
    vec3 lightColor = glob.light.color;
    float lightIntensity = glob.light.intensity;

    Material mat = materialAlias[glob.materialBufferIndex].materials[dd.materialOffset];

    TextureIndex albedoIndex = textureIndexAlias[glob.textureBufferIndex].textureIndices[mat.albedoTextureOffset];
    vec4 albedo = texture(sampler2D(globalTextures[nonuniformEXT(albedoIndex.textureIndex)],
    globalSamplers[nonuniformEXT(albedoIndex.samplerIndex)]), inUV);

    if (albedo.a < 0.01) {
        discard;
    }

    TextureIndex normalTex = textureIndexAlias[glob.textureBufferIndex].textureIndices[mat.normalTextureOffset];
    vec3 normal = texture(sampler2D(globalTextures[nonuniformEXT(normalTex.textureIndex)],
    globalSamplers[nonuniformEXT(normalTex.samplerIndex)]), inUV).rgb;

    TextureIndex metallicRoughnessTex = textureIndexAlias[glob.textureBufferIndex].textureIndices[mat.metallicRoughnessTextureOffset];
    vec4 metallicRoughness = texture(sampler2D(globalTextures[nonuniformEXT(metallicRoughnessTex.textureIndex)],
    globalSamplers[nonuniformEXT(metallicRoughnessTex.samplerIndex)]), inUV);

    // green channel for roughness, clamped to 0.089 to avoid division by 0
    float perceivedRoughness = clamp(mat.roughnessFactor * metallicRoughness.g, 0.089, 1.0);
    float roughness = perceivedRoughness * perceivedRoughness;

    // blue channel for metallic
    float metallic = mat.metallicFactor * metallicRoughness.b;

    // default reflectance values per filament doc
    float reflectance = 0.04;
    vec3 f0 = 0.16 * reflectance * reflectance * (1.0 - metallic) + albedo.rgb * metallic;

    vec3 diffuseColor = (1.0 - metallic) * albedo.rgb;

    vec3 N = normalize(inTBN * (normal * 2.0 - 1.0));
    vec3 L = normalize(lightPos - inFragPos);
    vec3 V = normalize(glob.cameraPos.xyz - inFragPos);
    vec3 H = normalize(L + V);

    float NoV = abs(dot(N, V) + 1e-5);
    float NoL = clamp(dot(N, L), 0.0, 1.0);
    float NoH = clamp(dot(N, H), 0.0, 1.0);
    float LoH = clamp(dot(L, H), 0.0, 1.0);

    //    float D = D_GGX(NoH, roughness);
    //    vec3 F = F_Schlick(LoH, f0);
    //    float specularV = V_SmithGGXCorrelated(NoV, NoL, roughness);
    //
    //    vec3 Fr = (D * specularV) * F;
    //    vec3 Fd = diffuseColor * Fd_Lambert();
    //
    //    vec3 diffuse = Fd * (1 - F) * lightColor * lightIntensity * NoL;// Diffuse lighting
    //    vec3 specular = Fr * lightColor * lightIntensity * NoL;// Specular lighting

    vec3 kS = fresnelSchlickRoughness(NoV, f0, roughness);
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    vec3 irradiance = texture(samplerCube(cubemapTextures[glob.irradianceMapIndex],
    globalSamplers[glob.cubemapSamplerIndex]), N).rgb * NoL;

    vec3 r = reflect(-V, N);
    r.y *= -1;
    vec3 prefilteredColor = textureLod(samplerCube(cubemapTextures[glob.prefilteredCubeIndex],
    globalSamplers[glob.cubemapSamplerIndex]), r, lodFromRoughness(roughness)).rgb;

    vec2 envBrdf = texture(sampler2D(globalTextures[glob.brdfLutIndex], globalSamplers[0]), vec2(NoV, roughness)).xy;

    vec3 diffuse = irradiance * diffuseColor;
    vec3 specular = prefilteredColor * (kS * envBrdf.x + envBrdf.y);

    float shadow = filterPCF(inFragLightSpace, glob.shadowDepthTextureIndex, glob.shadowSamplerIndex);
    vec3 finalColor = (kD * diffuse + specular) * shadow;
    //    vec3 finalColor = (diffuse + specular);

    outColor = vec4(finalColor, 1.0);
    //    float lod = textureQueryLod(sampler2D(globalTextures[albedoIndex.textureIndex], globalSamplers[albedoIndex.samplerIndex]), inUV).x;
    //    outColor += vec4(0.2, 0.0, 0.0, 0.0) * lod;
}
