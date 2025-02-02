#include "GpuResources.h"

namespace Flare {

    VertexInputCI &VertexInputCI::addBinding(VkVertexInputBindingDescription binding) {
        vertexBindings.emplace_back(binding);
        return *this;
    }

    VertexInputCI &VertexInputCI::addAttribute(VkVertexInputAttributeDescription attribute) {
        vertexAttributes.emplace_back(attribute);
        return *this;
    }

    ColorBlendCI &ColorBlendCI::addAttachment(ColorBlendAttachment attachment) {
        attachments.emplace_back(attachment);
        return *this;
    }

    void ReflectOutput::addBinding(uint32_t set, uint32_t binding, uint32_t count, VkDescriptorType type,
                                   VkShaderStageFlags stageFlag) {
        if (!descriptorSets.contains(set)) {
            descriptorSets.insert({set, {}});
        }

        bool found = false;
        for (auto &b: descriptorSets.at(set)) {
            if (b.binding == binding) {
                b.stageFlags |= stageFlag;
                found = true;
            }
        }
        if (!found) {
            descriptorSets.at(set).emplace_back(
                    VkDescriptorSetLayoutBinding{
                            .binding = binding,
                            .descriptorType = type,
                            .descriptorCount = count,
                            .stageFlags = stageFlag,
                            .pImmutableSamplers = nullptr,
                    }
            );
        }
    }

    ShaderStageCI &ShaderStageCI::addBinary(ShaderBinary binary) {
        shaderBinaries.push_back(binary);
        return *this;
    }

    DescriptorSetCI &DescriptorSetCI::addBuffer(Handle<Buffer> handle, uint32_t binding) {
        samplers.emplace_back();
        textures.emplace_back();
        buffers.emplace_back(handle);
        bindings.emplace_back(binding);
        resourceCount++;
        return *this;
    }

    DescriptorSetCI &DescriptorSetCI::addTexture(Handle<Texture> handle, uint32_t binding) {
        samplers.emplace_back();
        textures.emplace_back(handle);
        buffers.emplace_back();
        bindings.emplace_back(binding);
        resourceCount++;
        return *this;
    }

    DescriptorSetCI &DescriptorSetCI::addTextureSampler(Handle<Texture> textureHandle, Handle<Sampler> samplerHandle,
                                                        uint32_t binding) {
        samplers.emplace_back(samplerHandle);
        textures.emplace_back(textureHandle);
        buffers.emplace_back();
        bindings.emplace_back(binding);
        resourceCount++;
        return *this;
    }
}
