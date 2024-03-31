#pragma once
#include <unordered_map>
#include <sstream>
#include <vector>
#include <filesystem>

#include <shaderc/shaderc.hpp>
#include <vulkan/vulkan.h>

#include "Engine/Render/Shader.h"

namespace RT::Vulkan
{

	class VulkanShader : public Shader
	{
	public:
		using ShaderStages = std::vector<VkPipelineShaderStageCreateInfo>;

		enum class Type : uint8_t
		{
			None,
			Vertex,
			TessControl,
			TessEvaulation,
			Geometry,
			Fragment,
			Compute
		};

	private:
		template <typename SourceType>
		using SourceMap = std::unordered_map<Type, SourceType>;
		using Path = std::filesystem::path;

	public:
		void use() const final;
		void unuse() const final;
		void destroy() final;
		const uint32_t getId() const final;

		void load(const std::string& shaderName) final;

		const ShaderStages& getStages() const { return shaderStages; }

	private:
		void setUniformImpl(const std::string& uniName, const int32_t size, const void* value) const final;
	
		SourceMap<std::stringstream> readSources() const;
		SourceMap<std::vector<char>> readBinaries() const;
		SourceMap<std::vector<uint32_t>> compileSources(const SourceMap<std::stringstream>& sources) const;
		void reflect(const Type type, const std::vector<uint32_t>& shaderData) const;

		static constexpr VkShaderStageFlagBits shaderType2VkType(const Type type);
		static constexpr shaderc_shader_kind shaderType2ShaderC(const Type type);
		static constexpr const char* shaderType2Suffix(const Type type);
		static constexpr const char* shaderType2String(const Type type);

	private:
		Path shaderPath = "";

		std::vector<VkShaderModule> shaderModules = {};
		ShaderStages shaderStages = {};

		static constexpr std::string_view shaderDir = "..\\Engine\\assets\\shaders\\";
	};

}
