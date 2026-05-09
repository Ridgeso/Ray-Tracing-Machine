#pragma once
#include <unordered_map>
#include <sstream>
#include <vector>
#include <filesystem>

#include <shaderc/shaderc.hpp>
#include <vulkan/vulkan.h>

namespace RT::Vulkan
{

	class Shader
	{
	public:
		using Stages = std::vector<VkPipelineShaderStageCreateInfo>;

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
        Shader(const Path& shaderName);
        ~Shader();

		void load(const Path& shaderName);
		bool isCompute() const { return VK_SHADER_STAGE_COMPUTE_BIT == stages[0].stage; }

		const Stages& getStages() const { return stages; }

	private:
		SourceMap<std::stringstream> readSources() const;
		SourceMap<std::vector<char>> readBinaries() const;
		SourceMap<std::vector<uint32_t>> compileSources(const SourceMap<std::stringstream>& sources) const;
		void reflect(const Type type, const std::vector<uint32_t>& shaderData) const;

	private:
		Path shaderPath = "";

		std::vector<VkShaderModule> shaderModules = {};
		Stages stages = {};

		static constexpr std::string_view shaderDir = "..\\Engine\\assets\\shaders\\";
	};

}
