#pragma once
#include <filesystem>
#include <string_view>
#include <glm/glm.hpp>

#include "Engine/Core/Base.h"

#include <imgui.h>

namespace RT
{

	enum class ImageFormat
	{
		R8, RGB8, RGBA8, RGBA32F, Depth
	};

	enum class ImageLayout
	{
		Undefined, General, ShaderRead
	};

	enum class ImageAccess
	{
		None, Write, Read
	};

	struct Texture
	{
		virtual ~Texture() = 0 {}

		virtual void setBuffer(const void* data) = 0;

		virtual const ImTextureID getTexId() const = 0;
		virtual const glm::uvec2 getSize() const = 0;
		
		virtual void transition(const ImageAccess imageAccess, const ImageLayout imageLayout) const = 0;
		virtual void barrier(const ImageAccess imageAccess, const ImageLayout imageLayout) const = 0;

		static Local<Texture> create(const std::filesystem::path& path);
		static Local<Texture> create(const glm::uvec2 size, const ImageFormat imageFormat);
	};

	namespace Utils
	{

		const std::string_view imageFormat2Str(const ImageFormat imageFormat);

	}

}
