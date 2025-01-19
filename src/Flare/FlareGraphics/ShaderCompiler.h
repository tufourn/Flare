#pragma once

#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-com-helper.h>
#include <filesystem>

namespace Flare {
    struct ShaderCompiler {
        void init();
        void shutdown();

        void compile(std::filesystem::path path);

        void diagnose(slang::IBlob* diagnosticsBlob);

        Slang::ComPtr<slang::IGlobalSession> globalSession;
    };
}
