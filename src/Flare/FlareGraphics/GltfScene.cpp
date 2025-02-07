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

        textures.resize(data->images_count);
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

                            textures[i] = gpu->createTexture(textureCI);

                            asyncLoader->uploadRequests.emplace_back(
                                    UploadRequest{
                                            .texture = textures[i],
                                            .data = stbData, // asyncLoader will free stbData
                                    }
                            );

                            free(imageData);
                        }
                    } else {
                        spdlog::error("Invalid embedded image uri");
                    }
                } else {
                    spdlog::error("todo: load image from image file");
                    // image file

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

        buffers.resize(data->buffer_views_count);
        for (size_t i = 0; i < data->buffer_views_count; i++) {
            cgltf_buffer_view &bufferView = data->buffer_views[i];

            BufferCI ci = {
                    .size = bufferView.size,
                    .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            };

            buffers[i] = gpu->createBuffer(ci);

            asyncLoader->uploadRequests.emplace_back(
                    UploadRequest{
                            .dstBuffer = buffers[i],
                            .data = bufferView.buffer + bufferView.offset,
                    }
            );
        }

        for (size_t i = 0; i < data->textures_count; i++) {
            cgltf_texture &gltfTexture = data->textures[i];

            if (gltfTexture.image) {
                uint32_t textureIndex = gltfTexture.image - data->images;
                Texture* texture = gpu->textures.get(textures[textureIndex]);

                if (gltfTexture.sampler) {
                    uint32_t samplerIndex = gltfTexture.sampler - data->samplers;
                    texture->sampler = samplers[samplerIndex];
                } else {
                    texture->sampler = gpu->defaultSampler;
                }
            }
        }

        materials.resize(data->materials_count);
        for (size_t i = 0; i < data->materials_count; i++) {
            cgltf_material &material = data->materials[i];

            if (material.has_pbr_metallic_roughness) {
                cgltf_pbr_metallic_roughness &metallicRoughness = material.pbr_metallic_roughness;

                materials[i].baseColorFactor = glm::make_vec4(metallicRoughness.base_color_factor);
                materials[i].metallicFactor = metallicRoughness.metallic_factor;
                materials[i].roughnessFactor = metallicRoughness.roughness_factor;

                if (metallicRoughness.base_color_texture.texture) {
                    materials[i].baseTextureOffset = metallicRoughness.base_color_texture.texture - data->textures;
                }

                if (metallicRoughness.metallic_roughness_texture.texture) {
                    materials[i].metallicRoughnessTextureOffset =
                            metallicRoughness.metallic_roughness_texture.texture - data->textures;
                }
            }

            if (material.normal_texture.texture) {
                materials[i].normalTextureOffset = material.normal_texture.texture - data->textures;
            }

            if (material.occlusion_texture.texture) {
                materials[i].occlusionTextureOffset = material.occlusion_texture.texture - data->textures;
            }

            if (material.emissive_texture.texture) {
                materials[i].emissiveTextureOffset = material.emissive_texture.texture - data->textures;
            }
        }

        meshes.resize(data->meshes_count);
        for (size_t i = 0; i < data->meshes_count; i++) {
            cgltf_mesh &mesh = data->meshes[i];

            for (size_t prim_i = 0; prim_i < mesh.primitives_count; prim_i++) {
                cgltf_primitive &primitive = mesh.primitives[prim_i];

                MeshPrimitive meshPrimitive;

                for (size_t attr_i = 0; attr_i < primitive.attributes_count; attr_i++) {
                    cgltf_attribute &attribute = primitive.attributes[attr_i];
                    cgltf_accessor &accessor = *attribute.data;

                    uint32_t bufferIndex = accessor.buffer_view - data->buffer_views;
                    int offset = static_cast<int>(accessor.offset);

                    switch (attribute.type) {
                        case cgltf_attribute_type_position: { // float3
                            meshPrimitive.positionBufferHandle = buffers[bufferIndex];
                            meshPrimitive.positionOffset = offset;
                            meshPrimitive.vertexCount = accessor.count;
                            break;
                        }
                        case cgltf_attribute_type_normal: { // float3
                            meshPrimitive.normalBufferHandle = buffers[bufferIndex];
                            meshPrimitive.normalOffset = offset;
                            break;
                        }
                        case cgltf_attribute_type_texcoord: { // assume float2 todo: other types, multiple texcoords?
                            meshPrimitive.texcoordBufferHandle = buffers[bufferIndex];
                            meshPrimitive.texcoordOffset = offset;
                        }
                        case cgltf_attribute_type_tangent: {
                            // todo
                            break;
                        }
                            //todo: handle other attrs
                        default:
                            break;
                    }
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

            for (size_t child_i = 0; child_i < node.children_count; child_i++) {
                nodes[i].children.push_back(&nodes[node.children[child_i] - data->nodes]);
                nodes[child_i].parent = &nodes[i];
            }

            nodes[i].translation = {node.translation[0], node.translation[1], node.translation[2]};
            nodes[i].rotation = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
            nodes[i].scale = {node.scale[0], node.scale[1], node.scale[2]};
            nodes[i].matrix = glm::make_mat4(node.matrix);

            // todo: skins
        }

        for (const auto &node: nodes) {
            if (!node.parent) {
                topLevelNodes.push_back(&node);
            }
        }
    }

    void GltfScene::shutdown() {
        if (data) {
            cgltf_free(data);
        }

        for (auto &handle: textures) {
            gpu->destroyTexture(handle);
        }
        for (auto &handle: buffers) {
            gpu->destroyBuffer(handle);
        }
        for (auto &handle: samplers) {
            gpu->destroySampler(handle);
        }
    }

    void GltfScene::prepareDraws() {

    }

    glm::mat4 Node::getLocalTransform() const {
        glm::mat4 T = glm::translate(glm::mat4(1.f), translation);
        glm::mat4 R = glm::toMat4(rotation);
        glm::mat4 S = glm::scale(glm::mat4(1.f), scale);

        return T * R * S * matrix;
    }
}