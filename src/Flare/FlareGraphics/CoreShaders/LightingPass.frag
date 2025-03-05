#version 460

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#include "CoreShaders/BindlessCommon.glsl"
#include "CoreShaders/SrgbToLinear.glsl"
#include "CoreShaders/CubemapCommon.glsl"
#include "CoreShaders/NormalEncoding.glsl"

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

vec3 worldPositionFromDepth(mat4 viewProjectionInv, vec2 texturePos, float depth) {
    vec4 ndc = vec4((texturePos * 2) - 1.0, depth, 1.0);
    vec4 worldPos = viewProjectionInv * ndc;
    worldPos /= worldPos.w;

    return worldPos.xyz;
}

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

void main() {
    uint cameraBufferIndex = pc.data0;
    uint lightBufferIndex = pc.data1;
    uint albedoIndex = pc.data2;
    uint normalIndex = pc.data3;
    uint occlusionMetallicRoughnessIndex = pc.data4;
    uint emissiveIndex = pc.data5;
    uint depthIndex = pc.data6;

    Camera camera = cameraAlias[cameraBufferIndex].camera;
    Light light = lightAlias[lightBufferIndex].light;

    float depth = GET_TEXTURE(depthIndex, DEFAULT_SAMPLER_INDEX, inUV).r;
    vec3 worldPos = worldPositionFromDepth(camera.viewProjectionInv, inUV, depth);

    vec4 albedo = GET_TEXTURE(albedoIndex, DEFAULT_SAMPLER_INDEX, inUV);
    vec3 normal = octahedralDecode(GET_TEXTURE(normalIndex, DEFAULT_SAMPLER_INDEX, inUV).rg);
    vec3 occlusionMetallicRoughness = GET_TEXTURE(occlusionMetallicRoughnessIndex, DEFAULT_SAMPLER_INDEX, inUV).rgb;
    vec3 emissive = GET_TEXTURE(emissiveIndex, DEFAULT_SAMPLER_INDEX, inUV).rgb;

    float occlusion = occlusionMetallicRoughness.r;
    float metallic = occlusionMetallicRoughness.g;
    float roughness = occlusionMetallicRoughness.b;

    float alphaRoughness = roughness * roughness;

    vec3 f0 = vec3(0.04);

    vec3 diffuseColor = (1.0 - metallic) * (albedo.rgb * (vec3(1.0) - f0));
    vec3 specularColor = mix(f0, albedo.rgb, metallic);

    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
    vec3 specularEnvironmentR0 = specularColor.rgb;
    vec3 specularEnvironmentR90 = vec3(1.0) * reflectance90;

    vec3 cameraPos = camera.viewInv[3].xyz;

    vec3 N = normal;
    vec3 L = normalize(light.lightPos.xyz - worldPos);
    vec3 V = normalize(cameraPos.xyz - worldPos);
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
    roughness,
    metallic,
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

    vec3 color = NoL * (diffuseContrib + specularContrib);

    //todo: shadows and ibl

    const float occlusionStrength = 1.0;
    color = mix(color, color * occlusion, occlusionStrength);

    color += emissive;

    outColor = vec4(color, 1.0);
}
