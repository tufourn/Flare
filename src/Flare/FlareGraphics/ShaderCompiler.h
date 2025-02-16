#pragma once

#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-com-helper.h>
#include <filesystem>

#include "GpuResources.h"

namespace Flare {
    constexpr uint32_t SPIRV_MAGIC_NUMBER = 0x07230203;

    struct ShaderCompiler {
        void init();

        void shutdown();

        std::vector<uint32_t> compileSlang(const std::filesystem::path &path);

        static std::vector<uint32_t> compileGLSL(const std::filesystem::path &path);

        void diagnose(slang::IBlob *diagnosticsBlob);

        Slang::ComPtr <slang::IGlobalSession> globalSession;
    };
}
