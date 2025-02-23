#include "ShaderCompiler.h"
#include <spdlog/spdlog.h>
#include <shaderc/shaderc.hpp>

#include <array>
#include <fstream>

namespace Flare {
    void ShaderCompiler::init() {
        createGlobalSession(globalSession.writeRef());
    }

    void ShaderCompiler::shutdown() {

    }

    std::vector<uint32_t> ShaderCompiler::compileSlang(const fs::path &path) {
        if (!fs::exists(path)) {
            spdlog::error("ShaderCompiler: {} doesn't exist", path.string());
            return {};
        }

        slang::SessionDesc sessionDesc = {};
        slang::TargetDesc targetDesc = {};
        targetDesc.format = SLANG_SPIRV;
        targetDesc.profile = globalSession->findProfile("spirv_1_6");

        sessionDesc.targets = &targetDesc;
        sessionDesc.targetCount = 1;

        std::array<slang::CompilerOptionEntry, 2> options = {
                slang::CompilerOptionEntry{
                        slang::CompilerOptionName::EmitSpirvDirectly,
                        slang::CompilerOptionValue{slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}
                },
                slang::CompilerOptionEntry{
                        slang::CompilerOptionName::VulkanUseEntryPointName,
                        slang::CompilerOptionValue{slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}
                },
        };

        sessionDesc.compilerOptionEntries = options.data();
        sessionDesc.compilerOptionEntryCount = options.size();

        std::string parentPath = path.parent_path().string();
        std::array<const char *, 1> searchPaths = {
                parentPath.c_str(),
        };
        sessionDesc.searchPaths = searchPaths.data();
        sessionDesc.searchPathCount = searchPaths.size();

        Slang::ComPtr<slang::ISession> session;
        globalSession->createSession(sessionDesc, session.writeRef());

        Slang::ComPtr<slang::IModule> slangModule;
        std::string moduleName = path.stem().string();
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            slangModule = session->loadModule(moduleName.c_str(), diagnosticsBlob.writeRef());
            diagnose(diagnosticsBlob);
            if (!slangModule) {
                spdlog::error("ShaderCompiler: Failed to load module");
                return {};
            }
        }

        std::vector<slang::IComponentType *> componentTypes = {
                slangModule,
        };

        int definedEntryPointCount = slangModule->getDefinedEntryPointCount();

        for (int i = 0; i < definedEntryPointCount; i++) {
            Slang::ComPtr<slang::IEntryPoint> entryPoint;
            slangModule->getDefinedEntryPoint(i, entryPoint.writeRef());
            componentTypes.emplace_back(entryPoint.get());
        }

        Slang::ComPtr<slang::IComponentType> composedProgram;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            SlangResult result = session->createCompositeComponentType(
                    componentTypes.data(),
                    static_cast<uint32_t>(componentTypes.size()),
                    composedProgram.writeRef(),
                    diagnosticsBlob.writeRef()
            );
            diagnose(diagnosticsBlob);
        }

        Slang::ComPtr<slang::IComponentType> linkedProgram;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            SlangResult result = composedProgram->link(
                    linkedProgram.writeRef(),
                    diagnosticsBlob.writeRef()
            );
            diagnose(diagnosticsBlob);
        }

        Slang::ComPtr<slang::IBlob> spirvCode;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            SlangResult result = linkedProgram->getTargetCode(
                    0,
                    spirvCode.writeRef(),
                    diagnosticsBlob.writeRef()
            );
            diagnose(diagnosticsBlob);
        }

        spdlog::info("ShaderCompiler: Compiled {} with {} bytes of SPIR-V", moduleName, spirvCode->getBufferSize());

        auto *spirvBinary = (uint32_t *) spirvCode->getBufferPointer();

        assert(spirvBinary[0] == SPIRV_MAGIC_NUMBER);

        return {spirvBinary, spirvBinary + spirvCode->getBufferSize() / sizeof(uint32_t)};
    }

    void ShaderCompiler::diagnose(slang::IBlob *diagnosticsBlob) {
        if (diagnosticsBlob != nullptr) {
            const char *msg = static_cast<const char *>(diagnosticsBlob->getBufferPointer());
            spdlog::error("ShaderCompiler: {}", msg);
        }
    }

    std::vector<uint32_t> ShaderCompiler::compileGLSL(const fs::path &path) {
        std::vector<uint32_t> spirv;

        fs::path spvPath = path;
        spvPath += ".spv";

//      use cached spirv if exists and newer than glsl
        if (fs::exists(spvPath) && fs::last_write_time(spvPath) >= fs::last_write_time(path)) {
            std::ifstream spvFile(spvPath, std::ios::binary | std::ios::ate);
            if (!spvFile.is_open()) {
                spdlog::error("Failed to open cached SPIR-V file: {}", spvPath.string());
                return {};
            }

            size_t fileSize = spvFile.tellg();
            spvFile.seekg(0);

            spirv.resize(fileSize / sizeof(uint32_t));
            spvFile.read(reinterpret_cast<char *>(spirv.data()), fileSize);
            spvFile.close();

            spdlog::info("Loaded SPIR-V from {} ({} bytes)", spvPath.string(), fileSize);
            return spirv;
        }

        // otherwise compile
        shaderc::CompileOptions options;

        std::ifstream shaderFile(path);
        if (!shaderFile.is_open()) {
            spdlog::error("Failed to open {}", path.string());
            return {};
        }

        std::stringstream buf;
        buf << shaderFile.rdbuf();
        std::string shaderCode = buf.str();

        shaderFile.close();

        shaderc_shader_kind shaderKind;
        if (path.extension() == ".vert") {
            shaderKind = shaderc_glsl_vertex_shader;
        } else if (path.extension() == ".frag") {
            shaderKind = shaderc_glsl_fragment_shader;
        } else if (path.extension() == ".comp") {
            shaderKind = shaderc_glsl_compute_shader;
        } else {
            spdlog::error("Unsupported glsl shader type");
            return {};
        }

        shaderc::SpvCompilationResult result = shadercCompiler.CompileGlslToSpv(shaderCode, shaderKind,
                                                                                path.string().c_str(), options);

        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            spdlog::error("Shader compilation failed: {}", result.GetErrorMessage());
            return {};
        }

        spirv.assign(result.cbegin(), result.cend());

        assert(spirv[0] == SPIRV_MAGIC_NUMBER);

        spdlog::info("ShaderCompiler: Compiled {} ({} bytes)", path.string(), spirv.size() * sizeof(uint32_t));

        std::ofstream spvFile(spvPath, std::ios::binary);
        if (!spvFile.is_open()) {
            spdlog::error("Failed to save compiled SPIR-V file: {}", spvPath.string());
        } else {
            spvFile.write(reinterpret_cast<const char *>(spirv.data()), spirv.size() * sizeof(uint32_t));
            spvFile.close();
            spdlog::info("Saved SPIR-V to: {}", spvPath.string());
        }

        return spirv;
    }
}
