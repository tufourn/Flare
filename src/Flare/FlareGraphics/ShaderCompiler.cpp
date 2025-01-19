#include "ShaderCompiler.h"
#include <spdlog/spdlog.h>

#include <array>

namespace Flare {

    void ShaderCompiler::init() {
        createGlobalSession(globalSession.writeRef());
    }

    void ShaderCompiler::shutdown() {

    }

    void ShaderCompiler::compile(std::filesystem::path path) {
        if (!std::filesystem::exists(path)) {
            spdlog::error("ShaderCompiler: {} doesn't exist", path.string());
            return;
        }

        slang::SessionDesc sessionDesc = {};
        slang::TargetDesc targetDesc = {};
        targetDesc.format = SLANG_SPIRV;
        targetDesc.profile = globalSession->findProfile("spirv_1_5");

        sessionDesc.targets = &targetDesc;
        sessionDesc.targetCount = 1;

        std::array<slang::CompilerOptionEntry, 1> options = {
                {
                        slang::CompilerOptionName::EmitSpirvDirectly,
                        {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}
                }
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
        std::string moduleName = path.string();
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            slangModule = session->loadModule(moduleName.c_str(), diagnosticsBlob.writeRef());
            diagnose(diagnosticsBlob);
            if (!slangModule) {
                spdlog::error("ShaderCompiler: Failed to load module");
                return;
            }
        }

        Slang::ComPtr<slang::IEntryPoint> computeEntryPoint = nullptr;
        {
            SlangResult result = slangModule->findEntryPointByName("computeMain", computeEntryPoint.writeRef());
        }

        std::array<slang::IComponentType *, 2> componentTypes = {
                slangModule,
                computeEntryPoint,
        };

        Slang::ComPtr<slang::IComponentType> composedProgram;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            SlangResult result = session->createCompositeComponentType(
                    componentTypes.data(),
                    componentTypes.size(),
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
            SlangResult result = linkedProgram->getEntryPointCode(
                    0,
                    0,
                    spirvCode.writeRef(),
                    diagnosticsBlob.writeRef()
            );
            diagnose(diagnosticsBlob);
        }

        spdlog::info("ShaderCompiler: Compiled {} with {} bytes of SPIR-V", moduleName, spirvCode->getBufferSize());
    }

    void ShaderCompiler::diagnose(slang::IBlob *diagnosticsBlob) {
        if (diagnosticsBlob != nullptr) {
            const char *msg = static_cast<const char *>(diagnosticsBlob->getBufferPointer());
            spdlog::error("ShaderCompiler: {}", msg);
        }
    }
}
