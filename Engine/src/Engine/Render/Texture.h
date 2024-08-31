#pragma once
#include <filesystem>
#include <string_view>
#include <glm/glm.hpp>

#include "Engine/Core/Base.h"

#include <imgui.h>

namespace RT
{

	struct Texture
	{
		enum class Format { R8, RGB8, RGBA8, RGBA32F, Depth };
		enum class Layout { Undefined, General, ShaderRead };
		enum class Access { None, Write, Read };
		enum class Filter { None, Nearest, Linear };
		enum class Mode { Repeat, Mirrored, ClampToEdge, ClampToBorder };

		virtual ~Texture() = 0 {}

		virtual void setBuffer(const void* data) = 0;

		virtual const ImTextureID getTexId() const = 0;
		virtual const glm::uvec2 getSize() const = 0;
		
		virtual void transition(const Access imageAccess, const Layout imageLayout) const = 0;
		virtual void barrier(const Access imageAccess, const Layout imageLayout) const = 0;

		static Local<Texture> create(
			const std::filesystem::path& path,
			const Filter filter = Filter::Linear,
			const Mode mode = Mode::Repeat);
		static Local<Texture> create(const glm::uvec2 size, const Format imageFormat);
	};

	using TextureArray = std::vector<Local<Texture>>;

	namespace Utils
	{

		const std::string_view imageFormat2Str(const Texture::Format imageFormat);

	}

}
