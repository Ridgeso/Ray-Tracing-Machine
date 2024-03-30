#include "OpenGlTexture.h"

#include <glad/glad.h>

namespace RT::OpenGl
{

	OpenGlTexture::OpenGlTexture(const glm::uvec2 size, const ImageFormat imageFormat)
		: texId{}, size{size}, imageFormat{imageFormat}
	{
		glGenTextures(1, &texId);
		glBindTexture(GL_TEXTURE_2D, texId);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTexImage2D(GL_TEXTURE_2D, 0, imageFormat2GlFormat(imageFormat), size.x, size.y, 0, GL_RGBA, GL_FLOAT, nullptr);
	}

	OpenGlTexture::~OpenGlTexture()
	{
		glDeleteTextures(1, &texId);
	}
	
	void OpenGlTexture::setBuff(const void* data)
	{
		glTextureSubImage2D(texId, 0, 0, 0, size.x, size.y, GL_RGBA32F, GL_UNSIGNED_BYTE, data);
	}

	void OpenGlTexture::bind(const uint32_t slot) const
	{
		glBindTextureUnit(slot, texId);
	}

	constexpr int32_t OpenGlTexture::imageFormat2GlFormat(const ImageFormat imageFormat)
	{
		switch (imageFormat)
		{
			case ImageFormat::R8:      return GL_RED;
			case ImageFormat::RGB8:	   return GL_RGB;
			case ImageFormat::RGBA8:   return GL_RGBA;
			case ImageFormat::RGBA32F: return GL_RGBA32F;
		}
		return 0;
	}

}
