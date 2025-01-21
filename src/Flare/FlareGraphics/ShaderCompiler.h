#pragma once

#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-com-helper.h>
#include <filesystem>
#include <unordered_map>

#include "GpuResources.h"

namespace Flare {

    struct ReflectOutput {
        std::unordered_map<uint32_t, DescriptorSetLayoutCI> descriptorSets;

        void addBinding(uint32_t set, uint32_t binding, uint32_t count,
                        VkDescriptorType type, VkShaderStageFlags stageFlag);
    };

    struct ShaderCompiler {
        void init();

        void shutdown();

        std::vector<uint32_t> compile(const std::filesystem::path &path);

        void reflect(ReflectOutput &reflection, const std::vector<uint32_t> &spirv, ExecModel execModel,
                     const char *entryPoint = "main") const;

        void diagnose(slang::IBlob *diagnosticsBlob);

        Slang::ComPtr <slang::IGlobalSession> globalSession;
    };
}
