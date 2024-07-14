#include "RenderApi.h"
#include "External/Render/OpenGl/OpenGlRenderer.h"
#include "External/Render/Vulkan/VulkanRenderApi.h"

namespace RT
{
	
	RenderApi::Api RenderApi::api = RenderApi::Api::Vulkan;

	Local<RenderApi> createRenderApi()
	{
		switch (RenderApi::api)
		{
			// case RenderAPI::OpenGL: return makeLocal<OpenGl::OpenGlRenderer>();
			case RenderApi::Api::Vulkan: return makeLocal<Vulkan::VulkanRenderApi>();
		}
		return nullptr;
	}

}
