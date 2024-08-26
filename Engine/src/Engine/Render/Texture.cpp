#include "RenderApi.h"
#include "Texture.h"

#include "External/Render/OpenGl/OpenGlTexture.h"
#include "External/Render/Vulkan/VulkanTexture.h"

namespace RT
{

	Local<Texture> Texture::create(const std::filesystem::path& path)
	{
		switch (RenderApi::api)
		{
			// case RenderApi::Api::OpenGL: return makeLocal<OpenGl::OpenGlTexture>(size, imageFormat);
			case RenderApi::Api::Vulkan: return makeLocal<Vulkan::VulkanTexture>(path);
		}
		return nullptr;
	}

	Local<Texture> Texture::create(const glm::uvec2 size, const ImageFormat imageFormat)
	{
		switch (RenderApi::api)
		{
			// case RenderApi::Api::OpenGL: return makeLocal<OpenGl::OpenGlTexture>(size, imageFormat);
			case RenderApi::Api::Vulkan: return makeLocal<Vulkan::VulkanTexture>(size, imageFormat);
		}
		return nullptr;
	}

	namespace Utils
	{

		const std::string_view imageFormat2Str(const ImageFormat imageFormat)
		{
			switch (imageFormat)
			{
				case ImageFormat::R8:      return "R8";
				case ImageFormat::RGB8:    return "RGB8";
				case ImageFormat::RGBA8:   return "RGBA8";
				case ImageFormat::RGBA32F: return "RGBA32F";
				case ImageFormat::Depth:   return "Depth";
			}
			return "";
		}

	}

}
