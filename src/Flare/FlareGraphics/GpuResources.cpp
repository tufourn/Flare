#include "GpuResources.h"

namespace Flare {

    ShaderStageCI &ShaderStageCI::addStage(Shader stage) {
        shaders.emplace_back(stage);
        return *this;
    }

    VertexInputCI &VertexInputCI::addBinding(VkVertexInputBindingDescription binding) {
        vertexBindings.emplace_back(binding);
        return *this;
    }

    VertexInputCI &VertexInputCI::addAttribute(VkVertexInputAttributeDescription     attribute) {
        vertexAttributes.emplace_back(attribute);
        return *this;
    }

    ColorBlendCI &ColorBlendCI::addAttachment(ColorBlendAttachment attachment) {
        attachments.emplace_back(attachment);
        return *this;
    }
}
