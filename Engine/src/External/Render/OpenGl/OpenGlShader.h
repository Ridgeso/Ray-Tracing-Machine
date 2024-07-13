#pragma once
#include <string>
#include <cstdint>
#include <unordered_map>

#include <glad/glad.h>
#include <glm/glm.hpp>

//#include "Engine/Render/Shader.h"

namespace RT::OpenGl
{

	class OpenGlShader // : public Shader
	{
	public:
		enum Type
		{
			None,
			Vertex,
			TessEvaulation,
			TessControl,
			Geometry,
			Fragment,
			Compute
		};

	public:
		OpenGlShader() = default;
		~OpenGlShader() = default;

		void use() const; // final;
		void unuse() const; // final;
		void destroy(); // final;
		const uint32_t getId() const /*final*/ { return programId; }

		void load(const std::string& shaderPath); // final;
		
	private:
		std::unordered_map<Type, std::stringstream> readSources(const std::string& shaderPath) const;
		uint32_t compile(const Type type, const std::string& source) const;
		void reflect();
		void logLinkError(const std::string& shaderPath) const;

		static constexpr uint32_t shaderType2GlType(const Type type);

	private:
		uint32_t programId;
		std::unordered_map<std::string, int32_t> uniforms;
		std::unordered_map<int32_t, uint32_t> storages;
	};

}
