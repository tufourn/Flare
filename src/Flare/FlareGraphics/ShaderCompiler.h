#pragma once

#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-com-helper.h>
#include <filesystem>
#include <unordered_map>

#include "GpuResources.h"

namespace Flare {
    struct ShaderCompiler {
        void init();

        void shutdown();

        std::vector<uint32_t> compile(const std::filesystem::path &path);

        void diagnose(slang::IBlob *diagnosticsBlob);

        Slang::ComPtr <slang::IGlobalSession> globalSession;
    };
}
