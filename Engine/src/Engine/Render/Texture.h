#pragma once
#include <Engine/Core/Base.h>
#include <glm/glm.hpp>
#include <imgui.h>

namespace RT
{

	enum class ImageFormat
	{
		R8, RGB8, RGBA8, RGBA32F, Depth
	};

	struct Texture
	{
		virtual ~Texture() = 0;

		virtual void setBuff(const void* data) = 0;

		virtual const ImTextureID getTexId() const = 0;
		virtual const glm::uvec2 getSize() const = 0;

		static Local<Texture> create(const glm::uvec2 size, const ImageFormat imageFormat);
	};

}
