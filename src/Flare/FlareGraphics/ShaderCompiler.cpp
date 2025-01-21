#include "ShaderCompiler.h"
#include <spdlog/spdlog.h>
#include <spirv_cross.hpp>

#include <array>
#include <bit>

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
                        slang::CompilerOptionName::VulkanEmitReflection,
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

    void ShaderCompiler::reflect(ReflectOutput &reflection, const std::vector<uint32_t> &spirv, ExecModel execModel,
                                 const char *entryPoint) const {
        using namespace spirv_cross;
        std::vector<ExecModel> execModels = {ExecModel::eVertex, ExecModel::eFragment, ExecModel::eCompute};

        Compiler comp(spirv);
        auto entryPoints = comp.get_entry_points_and_stages();

        for (const auto &model: execModels) {
            if ((model & execModel) != ExecModel::eNone) {
                spv::ExecutionModel spvExecModel = spv::ExecutionModelMax;
                VkShaderStageFlags stageFlag = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
                switch (model) {
                    case ExecModel::eVertex:
                        spvExecModel = spv::ExecutionModelVertex;
                        stageFlag = VK_SHADER_STAGE_VERTEX_BIT;
                        break;
                    case ExecModel::eFragment:
                        spvExecModel = spv::ExecutionModelFragment;
                        stageFlag = VK_SHADER_STAGE_FRAGMENT_BIT;
                        break;
                    case ExecModel::eCompute:
                        spvExecModel = spv::ExecutionModelGLCompute;
                        stageFlag = VK_SHADER_STAGE_COMPUTE_BIT;
                        break;
                    default:
                        spdlog::error("ShaderCompiler: Invalid execution model");
                        break;
                }
                comp.set_entry_point(entryPoint, spvExecModel);

                ShaderResources res = comp.get_shader_resources();

                for (auto &uniform: res.uniform_buffers) {
                    uint32_t set = comp.get_decoration(uniform.id, spv::DecorationDescriptorSet);
                    uint32_t binding = comp.get_decoration(uniform.id, spv::DecorationBinding);

                    spdlog::info("ShaderCompiler: Found UBO {} at set = {}, binding = {}", uniform.name, set, binding);
                    reflection.addBinding(set, binding, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stageFlag);
                }
            }
        }
    }

    void ReflectOutput::addBinding(uint32_t set, uint32_t binding, uint32_t count, VkDescriptorType type,
                                   VkShaderStageFlags stageFlag) {
        if (!descriptorSets.contains(set)) {
            descriptorSets.insert({set, {}});
            descriptorSets.at(set).index = set;
        }

        bool found = false;
        for (auto &b: descriptorSets.at(set).bindings) {
            if (b.binding == binding) {
                b.stageFlags |= stageFlag;
                found = true;
            }
        }
        if (!found) {
            descriptorSets.at(set).bindings.emplace_back(
                    VkDescriptorSetLayoutBinding{
                            .binding = binding,
                            .descriptorType = type,
                            .descriptorCount = count,
                            .stageFlags = stageFlag,
                            .pImmutableSamplers = nullptr,
                    }
            );
        }
    }
}
