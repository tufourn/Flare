#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout (set = 2, binding = 0) uniform texture2D globalTextures[];
layout (set = 3, binding = 0) uniform writeonly image2D globalStorageImages[];
layout (set = 4, binding = 0) uniform sampler globalSamplers[];

layout (location = 0) in vec3 inFragPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;
layout (location = 3) flat in uint inDrawID;

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
    float pad;

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

void main() {
    Globals glob = globalBuffer[pc.globalIndex].globals;
    GpuMeshDraw md = meshDrawBuffer[glob.meshDrawBufferIndex].meshDraws[inDrawID];

    Material mat = materialBuffer[glob.materialBufferIndex].materials[md.materialOffset];

    TextureIndex tex = textureIndexBuffer[glob.textureBufferIndex].textureIndices[mat.albedoTextureOffset];

    vec4 fragColor = texture(sampler2D(globalTextures[nonuniformEXT(tex.textureIndex)],
    globalSamplers[nonuniformEXT(tex.samplerIndex)]), inUV);

    if (fragColor.a < 0.01) {
        discard;
    }

    vec3 lightPos = glob.light.position;
    vec3 lightColor = glob.light.color;
    float lightIntensity = glob.light.intensity;

    vec3 albedo = fragColor.rgb;

    vec3 N = normalize(inNormal);
    vec3 L = normalize(lightPos - inFragPos);
    vec3 V = normalize(-inFragPos);
    vec3 H = normalize(L + V);
    vec3 ambient = 0.1 * albedo;
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * albedo * lightColor * lightIntensity;
    float shininess = 32.0;
    float spec = pow(max(dot(N, H), 0.0), shininess);
    vec3 specular = spec * lightColor * lightIntensity;

    vec3 finalColor = ambient + diffuse + specular;
    outColor = vec4(finalColor, fragColor.a);
}
