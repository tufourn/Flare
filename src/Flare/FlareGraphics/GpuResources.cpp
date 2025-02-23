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
}
