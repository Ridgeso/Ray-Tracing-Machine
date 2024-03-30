#pragma once
#include <cstdint>
#include <filesystem>

#include <glm/glm.hpp>

#include "Engine/Render/Texture.h"

namespace RT::OpenGl
{

	class OpenGlTexture : public Texture
	{
		using path = std::filesystem::path;

	public:
		OpenGlTexture(const glm::uvec2 size, const ImageFormat imageFormat);
		~OpenGlTexture() final;

		void setBuff(const void* data) final;

		void bind(const uint32_t slot = 0) const final;

		const ImTextureID getTexId() const final { return (ImTextureID)texId; }
		const glm::uvec2 getSize() const final { return size; }

		const uint32_t getId() const { return texId; }

	private:
		constexpr int32_t imageFormat2GlFormat(const ImageFormat imageFormat);

	private:
		uint32_t texId;
		glm::uvec2 size;
		ImageFormat imageFormat;
	};

}
