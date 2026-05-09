#include "Texture.h"
#include "RenderApi.h"

//#include "External/Render/OpenGl/OpenGlTexture.h"
#include "External/Render/Vulkan/VulkanTexture.h"

namespace RT
{

	std::unique_ptr<Texture> Texture::create(const std::filesystem::path& path, const Filter filter, const Mode mode)
	{
		switch (RenderApi::api)
		{
			// case RenderApi::Api::OpenGL: return std::make_unique<OpenGl::OpenGlTexture>(size, imageFormat);
			case RenderApi::Api::Vulkan: return std::make_unique<Vulkan::VulkanTexture>(path, filter, mode);
		}
		return nullptr;
	}

	std::unique_ptr<Texture> Texture::create(const glm::uvec2 size, const Format imageFormat)
	{
		switch (RenderApi::api)
		{
			// case RenderApi::Api::OpenGL: return std::make_unique<OpenGl::OpenGlTexture>(size, imageFormat);
			case RenderApi::Api::Vulkan: return std::make_unique<Vulkan::VulkanTexture>(size, imageFormat);
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
