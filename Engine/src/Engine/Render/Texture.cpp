#include "RenderApi.h"
#include "Texture.h"

#include "External/Render/OpenGl/OpenGlTexture.h"
#include "External/Render/Vulkan/VulkanTexture.h"

namespace RT
{
	
	Local<Texture> Texture::create(const glm::uvec2 size, const ImageFormat imageFormat)
	{
		switch (RenderApi::api)
		{
			// case RenderApi::Api::OpenGL: return makeLocal<OpenGl::OpenGlTexture>(size, imageFormat);
			case RenderApi::Api::Vulkan: return makeLocal<Vulkan::VulkanTexture>(size, imageFormat);
		}
		return nullptr;
	}

	Texture::~Texture() { }

}
