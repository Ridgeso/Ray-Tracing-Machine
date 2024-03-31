#include "VulkanShader.h"
#include "Device.h"

#include <fstream>
#include <unordered_set>

#include <spirv_cross/spirv_cross.hpp>

#include "Engine/Core/Assert.h"

namespace RT::Vulkan
{

	void VulkanShader::use() const
	{
	}

	void VulkanShader::unuse() const
	{
	}

	void VulkanShader::destroy()
	{
        for (auto shaderModule : shaderModules)
        {
            vkDestroyShaderModule(DeviceInstance.getDevice(), shaderModule, nullptr);
        }
	}

	const uint32_t VulkanShader::getId() const
	{
		return 0;
	}

	void VulkanShader::load(const std::string& shaderName)
	{
        //shaderPath = Path{""} / shaderDir / shaderName;
        shaderPath = shaderName;

		auto shadersSources = readSources();
        RT_CORE_ASSERT(shadersSources.size() != 0, "Failed to find source code!");
        auto compiledSources = compileSources(shadersSources);

        shaderModules.reserve(compiledSources.size());
        shaderStages.reserve(compiledSources.size());
        for (const auto& [type, source] : compiledSources)
        {
            auto createInfo = VkShaderModuleCreateInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = source.size() * sizeof(uint32_t);
            createInfo.pCode = source.data();

            auto& shaderModule = shaderModules.emplace_back();

            RT_CORE_ASSERT(
                vkCreateShaderModule(DeviceInstance.getDevice(), &createInfo, nullptr, &shaderModule) == VK_SUCCESS,
                "failed to create shader module");
        
            auto& shaderStage = shaderStages.emplace_back();
            shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStage.stage = shaderType2VkType(type);
            shaderStage.module = shaderModule;
            shaderStage.pName = "main";
            shaderStage.flags = 0;
            shaderStage.pNext = nullptr;
            shaderStage.pSpecializationInfo = nullptr;
        }

        //TODO: full implementation with setUniformImpl
        //for (const auto& [stage, data] : compiledSources)
        //{
        //    reflect(stage, data);
        //}
	}

	void VulkanShader::setUniformImpl(const std::string& uniName, const int32_t size, const void* value) const
	{
	}

    VulkanShader::SourceMap<std::stringstream> VulkanShader::readSources() const
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
                    : line.find("FRAGMENT") != std::string::npos ? Type::Compute
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
    VulkanShader::SourceMap<std::vector<char>> VulkanShader::readBinaries() const
    {
        auto file = std::ifstream(shaderPath, std::ios::ate | std::ios::binary);

        RT_CORE_ASSERT(file.is_open(), "failed to open shader binary: {}", shaderPath);

        auto fileSize = static_cast<size_t>(file.tellg());
        auto buffer = std::vector<char>(fileSize);
        
        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return {};
    }

    VulkanShader::SourceMap<std::vector<uint32_t>> VulkanShader::compileSources(const SourceMap<std::stringstream>& sources) const
    {
        auto compiler = shaderc::Compiler{};
        auto options = shaderc::CompileOptions{};

        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
        constexpr bool optimize = true;
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
            
            RT_CORE_ASSERT(
                shaderModule.GetCompilationStatus() == shaderc_compilation_status_success,
                "Compilation Errors:\n{}",
                shaderModule.GetErrorMessage().c_str());

            sourceMap[type] = std::vector<uint32_t>(shaderModule.begin(), shaderModule.end());
        }

        return sourceMap;
    }

    void VulkanShader::reflect(const Type type, const std::vector<uint32_t>& shaderData) const
    {
        auto compiler = spirv_cross::Compiler(shaderData);
        auto resources = compiler.get_shader_resources();

        RT_LOG_TRACE("Shader Reflect [{}]: {}", shaderType2String(type), shaderPath);
        RT_LOG_TRACE("-   {} - uniform buffers", resources.uniform_buffers.size());
        RT_LOG_TRACE("-   {} - resources", resources.sampled_images.size());

        RT_LOG_TRACE("Uniform buffers:");
        for (const auto& resource : resources.uniform_buffers)
        {
            const auto& bufferType = compiler.get_type(resource.base_type_id);
            uint32_t bufferSize = compiler.get_declared_struct_size(bufferType);
            uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
            int32_t memberCount = bufferType.member_types.size();

            RT_LOG_TRACE("- {}", resource.name);
            RT_LOG_TRACE("-   Size = {}", bufferSize);
            RT_LOG_TRACE("-   Binding = {}", binding);
            RT_LOG_TRACE("-   Members = {}", memberCount);
        }
    }

    constexpr VkShaderStageFlagBits VulkanShader::shaderType2VkType(const Type type)
    {
        switch (type)
        {
            case Type::Vertex:         return VK_SHADER_STAGE_VERTEX_BIT;
            case Type::TessControl:    return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            case Type::TessEvaulation: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            case Type::Geometry:       return VK_SHADER_STAGE_GEOMETRY_BIT;
            case Type::Fragment:       return VK_SHADER_STAGE_FRAGMENT_BIT;
            case Type::Compute:        return VK_SHADER_STAGE_COMPUTE_BIT;
        }
        return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
    }

    constexpr shaderc_shader_kind VulkanShader::shaderType2ShaderC(const Type type)
    {
        switch (type)
        {
            case Type::Vertex:         return shaderc_vertex_shader;
            case Type::TessControl:    return shaderc_tess_control_shader;
            case Type::TessEvaulation: return shaderc_tess_evaluation_shader;
            case Type::Geometry:       return shaderc_geometry_shader;
            case Type::Fragment:       return shaderc_fragment_shader;
            case Type::Compute:        return shaderc_compute_shader;
        }
        return static_cast<shaderc_shader_kind>(0xFF);
    }

    constexpr const char* VulkanShader::shaderType2Suffix(const Type type)
    {
        switch (type)
        {
            case Type::Vertex:         return ".vert";
            case Type::TessControl:    return ".tesc";
            case Type::TessEvaulation: return ".tese";
            case Type::Geometry:       return ".geom";
            case Type::Fragment:       return ".frag";
            case Type::Compute:        return ".comp";
        }
        return "";
    }

    constexpr const char* VulkanShader::shaderType2String(const Type type)
    {
        #define TypeAsString(ShaderType) case Type:: ShaderType: return #ShaderType
        switch (type)
        {
            TypeAsString(Vertex);
            TypeAsString(TessControl);
            TypeAsString(TessEvaulation);
            TypeAsString(Geometry);
            TypeAsString(Fragment);
            TypeAsString(Compute);
        }
        return "";
        #undef TypeAsString
    }

}
