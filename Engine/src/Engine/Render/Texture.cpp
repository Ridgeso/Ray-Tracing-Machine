#include "RenderApi.h"
#include "Texture.h"

//#include "External/Render/OpenGl/OpenGlTexture.h"
#include "External/Render/Vulkan/VulkanTexture.h"

namespace RT
{

	Local<Texture> Texture::create(const std::filesystem::path& path, const Filter filter, const Mode mode)
	{
		switch (RenderApi::api)
		{
			// case RenderApi::Api::OpenGL: return makeLocal<OpenGl::OpenGlTexture>(size, imageFormat);
			case RenderApi::Api::Vulkan: return makeLocal<Vulkan::VulkanTexture>(path, filter, mode);
		}
		return nullptr;
	}

	Local<Texture> Texture::create(const glm::uvec2 size, const Format imageFormat)
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

		const std::string_view imageFormat2Str(const Texture::Format imageFormat)
		{
			switch (imageFormat)
			{
				case Texture::Format::R8:      return "R8";
				case Texture::Format::RGB8:    return "RGB8";
				case Texture::Format::RGBA8:   return "RGBA8";
				case Texture::Format::RGBA32F: return "RGBA32F";
				case Texture::Format::Depth:   return "Depth";
			}
			return "";
		}

	}

}
