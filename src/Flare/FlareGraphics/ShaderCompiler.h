#pragma once

#include <filesystem>
#include <shaderc/shaderc.hpp>
#include <slang-com-helper.h>
#include <slang-com-ptr.h>
#include <slang.h>

#include "GpuResources.h"

namespace fs = std::filesystem;

namespace Flare {
constexpr uint32_t SPIRV_MAGIC_NUMBER = 0x07230203;

struct ShaderIncluder : public shaderc::CompileOptions::IncluderInterface {
  shaderc_include_result *GetInclude(const char *requested_source,
                                     shaderc_include_type type,
                                     const char *requesting_source,
                                     size_t include_depth) override;

  void ReleaseInclude(shaderc_include_result *data) override;
};

struct ShaderCompiler {
  void init();

  void shutdown();

  std::vector<uint32_t> compileSlang(const fs::path &path);

  std::vector<uint32_t> compileGLSL(const fs::path &path);

  void diagnose(slang::IBlob *diagnosticsBlob);

  Slang::ComPtr<slang::IGlobalSession> globalSession;
  shaderc::Compiler shadercCompiler;
  shaderc::CompileOptions shadercOptions;
};
} // namespace Flare
