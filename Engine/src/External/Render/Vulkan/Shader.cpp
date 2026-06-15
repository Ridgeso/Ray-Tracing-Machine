#include "Shader.h"
#include <fstream>
#include <unordered_set>

#include <spirv_cross/spirv_cross.hpp>

#include "Device.h"
#include "utils/Debug.h"
#include "Engine/logging/Log.h"

namespace RT::Vulkan
{
namespace
{
    constexpr VkShaderStageFlagBits shaderType2VkType(const Shader::Type type)
    {
        switch (type)
        {
            case Shader::Type::Vertex:         return VK_SHADER_STAGE_VERTEX_BIT;
            case Shader::Type::TessControl:    return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            case Shader::Type::TessEvaulation: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            case Shader::Type::Geometry:       return VK_SHADER_STAGE_GEOMETRY_BIT;
            case Shader::Type::Fragment:       return VK_SHADER_STAGE_FRAGMENT_BIT;
            case Shader::Type::Compute:        return VK_SHADER_STAGE_COMPUTE_BIT;
        }
        return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
    }

    constexpr shaderc_shader_kind shaderType2ShaderC(const Shader::Type type)
    {
        switch (type)
        {
            case Shader::Type::Vertex:         return shaderc_vertex_shader;
            case Shader::Type::TessControl:    return shaderc_tess_control_shader;
            case Shader::Type::TessEvaulation: return shaderc_tess_evaluation_shader;
            case Shader::Type::Geometry:       return shaderc_geometry_shader;
            case Shader::Type::Fragment:       return shaderc_fragment_shader;
            case Shader::Type::Compute:        return shaderc_compute_shader;
        }
        return static_cast<shaderc_shader_kind>(0xFF);
    }

    constexpr const char* shaderType2Suffix(const Shader::Type type)
    {
        switch (type)
        {
            case Shader::Type::Vertex:         return ".vert";
            case Shader::Type::TessControl:    return ".tesc";
            case Shader::Type::TessEvaulation: return ".tese";
            case Shader::Type::Geometry:       return ".geom";
            case Shader::Type::Fragment:       return ".frag";
            case Shader::Type::Compute:        return ".comp";
        }
        return "";
    }

    constexpr const char* shaderType2String(const Shader::Type type)
    {
        switch (type)
        {
            case Shader::Type::Vertex:         return "Vertex";
            case Shader::Type::TessControl:    return "TessControl";
            case Shader::Type::TessEvaulation: return "TessEvaulation";
            case Shader::Type::Geometry:       return "Geometry";
            case Shader::Type::Fragment:       return "Fragment";
            case Shader::Type::Compute:        return "Compute";
        }
        return "";
    }
} // namespace

    Shader::Shader(const Path& shaderName)
    {
        load(shaderName);
    }

	Shader::~Shader()
	{
        for (auto shaderModule : shaderModules)
        {
            vkDestroyShaderModule(DeviceInstance.getDevice(), shaderModule, nullptr);
        }
	}

	void Shader::load(const Path& shaderName)
	{
        LOG_INFO("VULKAN", "Loading Shader: {{ path = {} }}", shaderName);
        //shaderPath = Path{""} / shaderDir / shaderName;
        shaderPath = shaderName;

		auto shadersSources = readSources();
        ASSERT("VULKAN", shadersSources.size() != 0, "Failed to find source code!");
        auto compiledSources = compileSources(shadersSources);

        shaderModules.reserve(compiledSources.size());
        stages.reserve(compiledSources.size());
        for (const auto& [type, source] : compiledSources)
        {
            auto createInfo = VkShaderModuleCreateInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = source.size() * sizeof(uint32_t);
            createInfo.pCode = source.data();

            auto& shaderModule = shaderModules.emplace_back();

            CHECK_VK(
                vkCreateShaderModule(DeviceInstance.getDevice(), &createInfo, nullptr, &shaderModule),
                "failed to create shader module");
        
            auto& stage = stages.emplace_back();
            stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stage.stage = shaderType2VkType(type);
            stage.module = shaderModule;
            stage.pName = "main";
            stage.flags = 0;
            stage.pNext = nullptr;
            stage.pSpecializationInfo = nullptr;
        }

        //TODO: full implementation with setUniformImpl
        //for (const auto& [stage, data] : compiledSources)
        //{
        //    reflect(stage, data);
        //}
        LOG_INFO("VULKAN", "Shader loaded");
    }

    Shader::SourceMap<std::stringstream> Shader::readSources() const
    {
        auto shaders = std::ifstream(shaderPath, std::ios::in);
        if (!shaders.is_open())
        {
            return {};
        }

        auto shadersSource = SourceMap<std::stringstream>();
        auto foundShaders = std::unordered_set<Type>();

        auto shaderType = Type::None;
        auto line = std::string();
        for (uint32_t lineNr = 1u; std::getline(shaders, line); lineNr++)
        {
            if (line.find("###SHADER") != std::string::npos)
            {
                shaderType
                    = line.find("VERTEX") != std::string::npos ? Type::Vertex
                    : line.find("TESS_CONTROL") != std::string::npos ? Type::TessControl
                    : line.find("TESS_EVAULATION") != std::string::npos ? Type::TessEvaulation
                    : line.find("GEOMETRY") != std::string::npos ? Type::Geometry
                    : line.find("FRAGMENT") != std::string::npos ? Type::Fragment
                    : line.find("COMPUTE") != std::string::npos ? Type::Compute
                    : Type::None;

                if (foundShaders.find(shaderType) == foundShaders.end())
                {
                    foundShaders.insert(shaderType);
                    shadersSource[shaderType] << std::string(lineNr, '\n');
                }
            }
            else if (shaderType != Type::None)
            {
                shadersSource[shaderType] << line << '\n';
            }
        }

        return shadersSource;
    }

    // TODO: when cashing shaders will be implemented
    Shader::SourceMap<std::vector<char>> Shader::readBinaries() const
    {
        auto file = std::ifstream(shaderPath, std::ios::ate | std::ios::binary);

        ASSERT("VULKAN", file.is_open(), "failed to open shader binary: {}", shaderPath);

        auto fileSize = static_cast<size_t>(file.tellg());
        auto buffer = std::vector<char>(fileSize);
        
        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return {};
    }

    Shader::SourceMap<std::vector<uint32_t>> Shader::compileSources(const SourceMap<std::stringstream>& sources) const
    {
        auto compiler = shaderc::Compiler{};
        auto options = shaderc::CompileOptions{};

        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_1);
        constexpr bool optimize = false;
        if (optimize)
        {
            options.SetOptimizationLevel(shaderc_optimization_level_performance);
        }

        auto sourceMap = SourceMap<std::vector<uint32_t>>();
        for (const auto& [type, source] : sources)
        {
            //auto dstPath = shaderPath.parent_path() / shaderPath.stem() / shaderType2suffix(type) / ".spv";

            auto shaderModule = compiler.CompileGlslToSpv(
                source.str(),
                shaderType2ShaderC(type),
                shaderPath.string().c_str(),
                "main",
                options);
            
            ASSERT(
                "VULKAN", 
                shaderModule.GetCompilationStatus() == shaderc_compilation_status_success,
                "Compilation Errors:\n{}",
                shaderModule.GetErrorMessage().c_str());

            sourceMap[type] = std::vector<uint32_t>(shaderModule.begin(), shaderModule.end());
        }

        return sourceMap;
    }

    void Shader::reflect(const Type type, const std::vector<uint32_t>& shaderData) const
    {
        auto compiler = spirv_cross::Compiler(shaderData);
        auto resources = compiler.get_shader_resources();

        LOG_TRACE("VULKAN", "Shader Reflect [{}]: {}", shaderType2String(type), shaderPath);
        LOG_TRACE("VULKAN", "-   {} - uniform buffers", resources.uniform_buffers.size());
        LOG_TRACE("VULKAN", "-   {} - resources", resources.sampled_images.size());

        LOG_TRACE("VULKAN", "Uniform buffers:");
        for (const auto& resource : resources.uniform_buffers)
        {
            const auto& bufferType = compiler.get_type(resource.base_type_id);
            uint32_t bufferSize = compiler.get_declared_struct_size(bufferType);
            uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
            int32_t memberCount = bufferType.member_types.size();

            LOG_TRACE("VULKAN", "- {}", resource.name);
            LOG_TRACE("VULKAN", "-   Size = {}", bufferSize);
            LOG_TRACE("VULKAN", "-   Binding = {}", binding);
            LOG_TRACE("VULKAN", "-   Members = {}", memberCount);
        }
    }

} // namespace RT::Vulkan
