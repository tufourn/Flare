#pragma once

#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-com-helper.h>
#include <filesystem>
#include <shaderc/shaderc.hpp>

#include "GpuResources.h"

namespace fs = std::filesystem;

namespace Flare {
    constexpr uint32_t SPIRV_MAGIC_NUMBER = 0x07230203;

    struct ShaderCompiler {
        void init();

        void shutdown();

        std::vector<uint32_t> compileSlang(const fs::path &path);

        std::vector<uint32_t> compileGLSL(const fs::path &path);

        void diagnose(slang::IBlob *diagnosticsBlob);

        Slang::ComPtr <slang::IGlobalSession> globalSession;
        shaderc::Compiler shadercCompiler;
    };
}
