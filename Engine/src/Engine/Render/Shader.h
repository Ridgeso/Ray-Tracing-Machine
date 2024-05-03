#pragma once
#include <type_traits>
#include <string>
#include <cstdint>
#include <Engine/Core/Base.h>

#include <glm/glm.hpp>

namespace RT
{

	struct Shader
	{
		virtual void use() const = 0;
		virtual void unuse() const = 0;
		virtual void destroy() = 0;
		virtual const uint32_t getId() const = 0;

		virtual void load(const std::string& shaderPath) = 0;
	};

	Local<Shader> createShader();

}
