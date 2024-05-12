#include "RenderBase.h"
#include "Texture.h"

#include "External/Render/OpenGl/OpenGlTexture.h"
#include "External/Render/Vulkan/VulkanTexture.h"

namespace RT
{
	
	Local<Texture> Texture::create(const glm::uvec2 size, const ImageFormat imageFormat)
	{
		switch (GlobalRenderAPI)
		{
			// case RenderAPI::OpenGL: return makeLocal<OpenGl::OpenGlTexture>(size, imageFormat);
			case RenderAPI::Vulkan: return makeLocal<Vulkan::VulkanTexture>(size, imageFormat);
		}
		return nullptr;
	}

	Texture::~Texture() { }

}
