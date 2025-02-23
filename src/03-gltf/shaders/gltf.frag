#version 460

#extension GL_EXT_nonuniform_qualifier : enable

#define PI 3.14159265

layout (set = 2, binding = 0) uniform texture2D globalTextures[];
layout (set = 3, binding = 0) uniform writeonly image2D globalStorageImages[];
layout (set = 4, binding = 0) uniform sampler globalSamplers[];

layout (location = 0) in vec3 inFragPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) flat in uint inDrawID;
layout (location = 3) in mat3 inTBN;

layout (location = 0) out vec4 outColor;

layout (push_constant) uniform PushConstants {
    uint globalIndex;
} pc;

struct Light {
    vec3 position;
    float radius;
    vec3 color;
    float intensity;
};

struct Globals {
    mat4 mvp;

    uint positionBufferIndex;
    uint normalBufferIndex;
    uint uvBufferIndex;
    uint transformBufferIndex;

    uint textureBufferIndex;
    uint materialBufferIndex;
    uint meshDrawBufferIndex;
    uint tangentBufferIndex;

    Light light;
};

struct Material {
    vec4 albedoFactor;

    uint albedoTextureOffset;
    uint metallicRoughnessTextureOffset;
    uint normalTextureOffset;
    uint occlusionTextureOffset;

    uint emissiveTextureOffset;
    float metallicFactor;
    float roughnessFactor;
    float pad;
};

layout (set = 0, binding = 0) uniform U { Globals globals; } globalBuffer[];

struct GpuMeshDraw {
    uint transformOffset;
    uint materialOffset;
};

struct TextureIndex {
    uint textureIndex;
    uint samplerIndex;
};

// aliasing ssbo
layout (set = 1, binding = 0) readonly buffer MeshDraws {
    GpuMeshDraw meshDraws[];
} meshDrawBuffer[];

layout (set = 1, binding = 0) readonly buffer T {
    TextureIndex textureIndices[];
} textureIndexBuffer[];

layout (set = 1, binding = 0) readonly buffer M {
    Material materials[];
} materialBuffer[];

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

void main() {
    Globals glob = globalBuffer[pc.globalIndex].globals;
    GpuMeshDraw md = meshDrawBuffer[glob.meshDrawBufferIndex].meshDraws[inDrawID];

    vec3 lightPos = glob.light.position;
    vec3 lightColor = glob.light.color;
    float lightIntensity = glob.light.intensity;

    Material mat = materialBuffer[glob.materialBufferIndex].materials[md.materialOffset];

    TextureIndex albedoIndex = textureIndexBuffer[glob.textureBufferIndex].textureIndices[mat.albedoTextureOffset];
    vec4 albedo = texture(sampler2D(globalTextures[nonuniformEXT(albedoIndex.textureIndex)],
    globalSamplers[nonuniformEXT(albedoIndex.samplerIndex)]), inUV);

    TextureIndex normalTex = textureIndexBuffer[glob.textureBufferIndex].textureIndices[mat.normalTextureOffset];
    vec3 normal = texture(sampler2D(globalTextures[nonuniformEXT(normalTex.textureIndex)],
    globalSamplers[nonuniformEXT(normalTex.samplerIndex)]), inUV).rgb;

    TextureIndex metallicRoughnessTex = textureIndexBuffer[glob.textureBufferIndex].textureIndices[mat.metallicRoughnessTextureOffset];
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
    vec3 V = normalize(-inFragPos);
    vec3 H = normalize(L + V);

    float NoV = abs(dot(N, V) + 1e-5);
    float NoL = clamp(dot(N, L), 0.0, 1.0);
    float NoH = clamp(dot(N, H), 0.0, 1.0);
    float LoH = clamp(dot(L, H), 0.0, 1.0);

    float D = D_GGX(NoH, roughness);
    vec3 F = F_Schlick(LoH, f0);
    float specularV = V_SmithGGXCorrelated(NoV, NoL, roughness);

    vec3 Fr = (D * specularV) * F;
    vec3 Fd = diffuseColor * Fd_Lambert();

    vec3 diffuse = Fd * lightColor * lightIntensity * NoL;  // Diffuse lighting
    vec3 specular = Fr * lightColor * lightIntensity * NoL;  // Specular lighting

    vec3 finalColor = diffuse + specular;

    outColor = vec4(finalColor, 1.0);
}
