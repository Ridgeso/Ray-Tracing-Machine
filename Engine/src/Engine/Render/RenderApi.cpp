#include "RenderApi.h"

#include "External/Render/OpenGl/OpenGlRenderer.h"
#include "External/Render/Vulkan/VulkanRenderApi.h"

namespace RT
{
	
	RenderApi::Api RenderApi::api = RenderApi::Api::Vulkan;

	std::unique_ptr<RenderApi> RenderApi::create()
	{
		switch (RenderApi::api)
		{
			// case RenderAPI::OpenGL: return std::make_unique<OpenGl::OpenGlRenderer>();
			case RenderApi::Api::Vulkan: return std::make_unique<Vulkan::VulkanRenderApi>();
		}
		return nullptr;
	}

}
