#include "ShaderCompiler.h"
#include <spdlog/spdlog.h>

#include <array>

namespace Flare {
    void ShaderCompiler::init() {
        createGlobalSession(globalSession.writeRef());
    }

    void ShaderCompiler::shutdown() {

    }

    std::vector<uint32_t> ShaderCompiler::compile(const std::filesystem::path &path) {
        if (!std::filesystem::exists(path)) {
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

        auto magicNumber = spirvBinary[0];
        assert(magicNumber == 0x07230203);

        return {spirvBinary, spirvBinary + spirvCode->getBufferSize() / sizeof(uint32_t)};
    }

    void ShaderCompiler::diagnose(slang::IBlob *diagnosticsBlob) {
        if (diagnosticsBlob != nullptr) {
            const char *msg = static_cast<const char *>(diagnosticsBlob->getBufferPointer());
            spdlog::error("ShaderCompiler: {}", msg);
        }
    }
}
