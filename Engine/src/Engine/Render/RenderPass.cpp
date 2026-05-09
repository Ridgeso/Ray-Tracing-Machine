#include "RenderPass.h"
#include "RenderApi.h"

#include "External/Render/OpenGl/OpenGlRenderPass.h"
#include "External/Render/Vulkan/VulkanRenderPass.h"

namespace RT
{

	std::shared_ptr<RenderPass> RenderPass::create(const RenderPassSpec& spec)
	{
		switch (RenderApi::api)
		{
			// case RenderApi::Api: return std::make_shared<OpenGl::OpenGlRenderPass>(spec);
			case RenderApi::Api::Vulkan: return std::make_shared<Vulkan::VulkanRenderPass>(spec);
		}
		return nullptr;
	}

}
