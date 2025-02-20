#include "GltfScene.h"
#include "AsyncLoader.h"
#include "GpuDevice.h"
#include "VkHelper.h"
#include "glm/gtc/type_ptr.hpp"
#include <stb_image.h>

namespace Flare {
    void GltfScene::init(const std::filesystem::path &path, GpuDevice *gpuDevice, AsyncLoader *asyncLoader) {
        gpu = gpuDevice;

        std::filesystem::path directory = path.parent_path();

        cgltf_options options = {};
        cgltf_result result = cgltf_parse_file(&options, path.string().c_str(), &data);
        if (result != cgltf_result_success) {
            spdlog::error("Failed to load gltf file {}", path.string());
            return;
        }
        cgltf_load_buffers(&options, data, path.string().c_str());

        images.resize(data->images_count);
        for (size_t i = 0; i < data->images_count; i++) {
            const cgltf_image *cgltfImage = &data->images[i];
            const char *uri = cgltfImage->uri;

            int width, height, channelCount;

            if (uri) {
                if (strncmp(uri, "data:", 5) == 0) {
                    // embedded image base64
                    const char *comma = strchr(uri, ',');
                    if (comma && comma - uri >= 7 && strncmp(comma - 7, ";base64", 7) == 0) {
                        const char *base64 = comma + 1;
                        const size_t base64Size = strlen(base64);
                        size_t decodedBinarySize = base64Size - base64Size / 4;

                        if (base64Size >= 2) {
                            decodedBinarySize -= base64[base64Size - 1] == '=';
                            decodedBinarySize -= base64[base64Size - 2] == '=';
                        }

                        void *imageData = nullptr;
                        cgltf_options base64Options = {};

                        if (cgltf_load_buffer_base64(&base64Options, decodedBinarySize, base64, &imageData) !=
                            cgltf_result_success) {
                            spdlog::error("Failed to parse base64 image uri");
                        } else {
                            unsigned char *stbData = stbi_load_from_memory(
                                    static_cast<const unsigned char *>(imageData),
                                    static_cast<int>(decodedBinarySize),
                                    &width, &height, &channelCount, STBI_rgb_alpha);

                            TextureCI textureCI = {
                                    .width = static_cast<uint16_t>(width),
                                    .height = static_cast<uint16_t>(height),
                                    .depth = 1,
                                    .format = VK_FORMAT_R8G8B8A8_UNORM,
                                    .type = VK_IMAGE_TYPE_2D,
                                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                            };

                            images[i] = gpu->createTexture(textureCI);

                            asyncLoader->uploadRequests.emplace_back(
                                    UploadRequest{
                                            .texture = gpu->getTexture(images[i]),
                                            .data = stbData, // asyncLoader will free stbData
                                    }
                            );

                            free(imageData);
                        }
                    } else {
                        spdlog::error("Invalid embedded image uri");
                    }
                } else {
                    std::filesystem::path imageFile = directory / uri;

                    stbi_info(imageFile.string().c_str(), &width, &height, &channelCount);

                    TextureCI textureCI = {
                            .width = static_cast<uint16_t>(width),
                            .height = static_cast<uint16_t>(height),
                            .depth = 1,
                            .format = VK_FORMAT_R8G8B8A8_UNORM,
                            .type = VK_IMAGE_TYPE_2D,
                            .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    };

                    images[i] = gpu->createTexture(textureCI);

                    asyncLoader->fileRequests.emplace_back(
                            FileRequest{
                                    .path = imageFile,
                                    .texture = gpu->getTexture(images[i]),
                            }
                    );
                }
            } else {
                spdlog::error("todo: load image from buffer");
                // image from buffer
            }
        }

        samplers.resize(data->samplers_count);
        for (size_t i = 0; i < data->samplers_count; i++) {
            cgltf_sampler &sampler = data->samplers[i];

            SamplerCI ci = {
                    .minFilter = VkHelper::extractGltfMinFilter(sampler.min_filter),
                    .magFilter = VkHelper::extractGltfMagFilter(sampler.mag_filter),
                    .mipFilter = VkHelper::extractGltfMipmapMode(sampler.min_filter),
                    .u = VkHelper::extractGltfWrapMode(sampler.wrap_s),
                    .v = VkHelper::extractGltfWrapMode(sampler.wrap_t),
                    .w = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            };

            samplers[i] = gpu->createSampler(ci);
        }

        uint32_t defaultAlbedoOffset = data->textures_count - 1 + DEFAULT_ALBEDO_BASE_OFFSET;
        uint32_t defaultNormalOffset = data->textures_count - 1 + DEFAULT_NORMAL_BASE_OFFSET;
        uint32_t defaultMetallicRoughnessOffset = data->textures_count - 1 + DEFAULT_METALLIC_ROUGHNESS_BASE_OFFSET;
        uint32_t defaultOcclusionOffset = data->textures_count - 1 + DEFAULT_OCCLUSION_BASE_OFFSET;
        uint32_t defaultEmissiveOffset = data->textures_count - 1 + DEFAULT_EMISSIVE_BASE_OFFSET;

        gltfTextures.resize(data->textures_count + 5); // make space for default textures at end

        gltfTextures[defaultAlbedoOffset] = {gpu->defaultTexture.index, gpu->defaultSampler.index};
        gltfTextures[defaultNormalOffset] = {gpu->defaultNormalTexture.index, gpu->defaultSampler.index};
        gltfTextures[defaultMetallicRoughnessOffset] = {gpu->defaultTexture.index, gpu->defaultSampler.index}; //todo
        gltfTextures[defaultOcclusionOffset] = {gpu->defaultTexture.index, gpu->defaultSampler.index}; //todo
        gltfTextures[defaultEmissiveOffset] = {gpu->defaultTexture.index, gpu->defaultSampler.index}; //todo

        for (size_t i = 0; i < data->textures_count; i++) {
            cgltf_texture &gltfTexture = data->textures[i];

            if (gltfTexture.image) {
                uint32_t textureIndex = gltfTexture.image - data->images;
                gltfTextures[i].imageIndex = images[textureIndex].index;
                if (gltfTexture.sampler) {
                    uint32_t samplerIndex = gltfTexture.sampler - data->samplers;
                    gltfTextures[i].samplerIndex = samplers[samplerIndex].index;
                } else {
                    gltfTextures[i].samplerIndex = gpu->defaultSampler.index;
                }
            } else {
                gltfTextures[i] = {
                        .imageIndex = gpu->defaultTexture.index,
                        .samplerIndex = gpu->defaultSampler.index,
                };
            }
        }

        materials.resize(data->materials_count + 1); // + 1 for default material at the back

        Material &defaultMaterial = materials.back();

        defaultMaterial = {
                .albedoFactor = {1.0, 1.0, 1.0, 1.0},
                .albedoTextureOffset = defaultAlbedoOffset,
                .metallicRoughnessTextureOffset = defaultMetallicRoughnessOffset,
                .normalTextureOffset = defaultNormalOffset,
                .occlusionTextureOffset = defaultOcclusionOffset,
                .emissiveTextureOffset = defaultEmissiveOffset,
                .metallicFactor = 1.f,
                .roughnessFactor = 1.f,
        };

        for (size_t i = 0; i < data->materials_count; i++) {
            cgltf_material &material = data->materials[i];

            if (material.has_pbr_metallic_roughness) {
                cgltf_pbr_metallic_roughness &metallicRoughness = material.pbr_metallic_roughness;

                materials[i].albedoFactor = glm::make_vec4(metallicRoughness.base_color_factor);
                materials[i].metallicFactor = metallicRoughness.metallic_factor;
                materials[i].roughnessFactor = metallicRoughness.roughness_factor;

                if (metallicRoughness.base_color_texture.texture) {
                    materials[i].albedoTextureOffset = metallicRoughness.base_color_texture.texture - data->textures;
                } else {
                    materials[i].albedoTextureOffset = defaultMaterial.albedoTextureOffset;
                }

                if (metallicRoughness.metallic_roughness_texture.texture) {
                    materials[i].metallicRoughnessTextureOffset =
                            metallicRoughness.metallic_roughness_texture.texture - data->textures;
                } else {
                    materials[i].metallicRoughnessTextureOffset = defaultMaterial.metallicRoughnessTextureOffset;
                }
            }

            if (material.normal_texture.texture) {
                materials[i].normalTextureOffset = material.normal_texture.texture - data->textures;
            } else {
                materials[i].normalTextureOffset = defaultMaterial.normalTextureOffset;
            }

            if (material.occlusion_texture.texture) {
                materials[i].occlusionTextureOffset = material.occlusion_texture.texture - data->textures;
            } else {
                materials[i].occlusionTextureOffset = defaultMaterial.occlusionTextureOffset;
            }

            if (material.emissive_texture.texture) {
                materials[i].emissiveTextureOffset = material.emissive_texture.texture - data->textures;
            } else {
                materials[i].emissiveTextureOffset = defaultMaterial.emissiveTextureOffset;
            }
        }

        meshes.resize(data->meshes_count);
        uint32_t meshPrimitiveId = 0;
        for (size_t i = 0; i < data->meshes_count; i++) {
            cgltf_mesh &mesh = data->meshes[i];

            for (size_t prim_i = 0; prim_i < mesh.primitives_count; prim_i++) {
                cgltf_primitive &primitive = mesh.primitives[prim_i];

                GltfMeshPrimitive meshPrimitive;
                meshPrimitive.id = meshPrimitiveId++;

                if (primitive.material) {
                    meshPrimitive.materialOffset = primitive.material - data->materials;
                } else {
                    meshPrimitive.materialOffset = materials.size() - 1; // default material at the back
                }

                if (primitive.indices) {
                    cgltf_accessor &accessor = *primitive.indices;
                    meshPrimitive.indices.resize(accessor.count);

                    cgltf_buffer_view &bufferView = *accessor.buffer_view;
                    size_t offset = accessor.offset + bufferView.offset;
                    const uint8_t *bufferData = static_cast<uint8_t *>(bufferView.buffer->data) + offset;

                    switch (cgltf_component_size(accessor.component_type)) {
                        case 4: {
                            for (size_t index = 0; index < accessor.count; index++) {
                                meshPrimitive.indices[index] = reinterpret_cast<const uint32_t *>(bufferData)[index];
                            }
                            break;
                        }
                        case 2: {
                            for (size_t index = 0; index < accessor.count; index++) {
                                meshPrimitive.indices[index] = reinterpret_cast<const uint16_t *>(bufferData)[index];
                            }
                            break;
                        }
                        case 1: {
                            for (size_t index = 0; index < accessor.count; index++) {
                                meshPrimitive.indices[index] = reinterpret_cast<const uint8_t *>(bufferData)[index];
                            }
                            break;
                        }
                        default:
                            spdlog::error("invalid primitive index component type");
                            break;
                    }
                }

                bool hasNormal = false;
                bool hasUV = false;
                bool hasTangent = false;

                for (size_t attr_i = 0; attr_i < primitive.attributes_count; attr_i++) {
                    cgltf_attribute &attribute = primitive.attributes[attr_i];

                    cgltf_accessor &accessor = *attribute.data;
                    cgltf_buffer_view &bufferView = *accessor.buffer_view;
                    size_t offset = accessor.offset + bufferView.offset;
                    const uint8_t *bufferData = static_cast<uint8_t *>(bufferView.buffer->data) + offset;

                    switch (attribute.type) {
                        case cgltf_attribute_type_position: { // float3, convert to vec of float4
                            meshPrimitive.positions.resize(accessor.count);
                            const float *positionBuffer = reinterpret_cast<const float *>(bufferData);

                            for (size_t pos_i = 0; pos_i < accessor.count; pos_i++) {
                                const float *pos = positionBuffer + pos_i * 3;
                                meshPrimitive.positions[pos_i] = glm::vec4(pos[0], pos[1], pos[2], 1.f);
                            }
                            break;
                        }
                        case cgltf_attribute_type_normal: { // float3, convert to vec of float4
                            hasNormal = true;
                            meshPrimitive.normals.resize(accessor.count);
                            const float *normalBuffer = reinterpret_cast<const float *>(bufferData);

                            for (size_t normal_i = 0; normal_i < accessor.count; normal_i++) {
                                const float *normal = normalBuffer + normal_i * 3;
                                meshPrimitive.normals[normal_i] = glm::vec4(normal[0], normal[1], normal[2], 1.f);
                            }
                            break;
                        }
                        case cgltf_attribute_type_texcoord: { // assume float2
                            hasUV = true;
                            meshPrimitive.uvs.resize(accessor.count);
                            memcpy(meshPrimitive.uvs.data(), bufferData, accessor.count * sizeof(glm::vec2));
                            break;
                        }
                        case cgltf_attribute_type_tangent: {
                            //todo
                            break;
                        }
                            // todo: handle weights and joints stuff for skinning
                        default:
                            break;
                    }
                }

                if (!hasNormal) {
                    meshPrimitive.normals = std::vector<glm::vec4>(meshPrimitive.positions.size());
                }

                if (!hasUV) {
                    meshPrimitive.uvs = std::vector<glm::vec2>(meshPrimitive.positions.size());
                }

                meshes[i].meshPrimitives.push_back(meshPrimitive);
            }
        }

        nodes.resize(data->nodes_count);
        for (size_t i = 0; i < data->nodes_count; i++) {
            cgltf_node &node = data->nodes[i];

            if (node.mesh) {
                nodes[i].mesh = &meshes[node.mesh - data->meshes];
            }

            if (node.skin) {
                nodes[i].skin = node.skin - data->skins; // todo
            }

            for (size_t child_i = 0; child_i < node.children_count; child_i++) {
                size_t childIndex = node.children[child_i] - data->nodes;
                nodes[i].children.push_back(&nodes[childIndex]);
                nodes[childIndex].parent = &nodes[i];
            }

            nodes[i].translation = {node.translation[0], node.translation[1], node.translation[2]};
            nodes[i].rotation = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
            nodes[i].scale = {node.scale[0], node.scale[1], node.scale[2]};
            nodes[i].matrix = glm::make_mat4(node.matrix);

            // todo: skins
        }

        std::unordered_map<uint32_t, std::vector<MeshDraw>> meshDrawsMap;
        for (auto &node: nodes) {
            if (!node.parent) {
                topLevelNodes.push_back(&node);
                generateMeshDrawsFromNode(&node, meshDrawsMap);
            }
        }

        for (const auto &pair: meshDrawsMap) {
            meshDraws.insert(meshDraws.end(), pair.second.begin(), pair.second.end());
        }
    }

    void GltfScene::shutdown() {
        if (data) {
            cgltf_free(data);
        }

        for (auto &handle: images) {
            gpu->destroyTexture(handle);
        }
        for (auto &handle: samplers) {
            gpu->destroySampler(handle);
        }
    }

    void GltfScene::generateMeshDrawsFromNode(Node *node, std::unordered_map<uint32_t, std::vector<MeshDraw>> &map) {
        node->updateWorldTransform();

        if (node->mesh) {
            for (const auto &meshPrim: node->mesh->meshPrimitives) {
                MeshDraw meshDraw;
                meshDraw.transformOffset = transforms.size();
                transforms.push_back(node->worldTransform);
                meshDraw.materialOffset = meshPrim.materialOffset;

                //todo: material

                if (!map.contains(meshPrim.id)) {
                    meshDraw.indexCount = meshPrim.indices.size();

                    meshDraw.indexOffset = indices.size();
                    indices.insert(indices.end(), meshPrim.indices.begin(), meshPrim.indices.end());

                    meshDraw.vertexOffset = positions.size();
                    positions.insert(positions.end(), meshPrim.positions.begin(), meshPrim.positions.end());
                    normals.insert(normals.end(), meshPrim.normals.begin(), meshPrim.normals.end());
                    uvs.insert(uvs.end(), meshPrim.uvs.begin(), meshPrim.uvs.end());

                    map.insert({meshPrim.id, {meshDraw}});
                } else { // reuse values from mesh primitive with same id //todo: skinned mesh with different offsets
                    meshDraw.indexCount = meshPrim.indices.size();
                    meshDraw.indexOffset = map.at(meshPrim.id).back().indexOffset;
                    meshDraw.vertexOffset = map.at(meshPrim.id).back().vertexOffset;

                    map.at(meshPrim.id).push_back(meshDraw);
                }
            }
        }
        for (const auto &child: node->children) {
            generateMeshDrawsFromNode(child, map);
        }
    }

    glm::mat4 Node::getLocalTransform() const {
        glm::mat4 T = glm::translate(glm::mat4(1.f), translation);
        glm::mat4 R = glm::toMat4(rotation);
        glm::mat4 S = glm::scale(glm::mat4(1.f), scale);

        return T * R * S * matrix;
    }

    void Node::updateWorldTransform() {
        if (parent) {
            worldTransform = parent->worldTransform * getLocalTransform();
        } else {
            worldTransform = getLocalTransform();
        }
    }
}