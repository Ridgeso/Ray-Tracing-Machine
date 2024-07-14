#include "RenderApi.h"
#include "RenderPass.h"

#include "External/Render/OpenGl/OpenGlRenderPass.h"
#include "External/Render/Vulkan/VulkanRenderPass.h"

namespace RT
{

	Share<RenderPass> RenderPass::create(const RenderPassSpec& spec)
	{
		switch (RenderApi::api)
		{
			// case RenderApi::Api: return makeShare<OpenGl::OpenGlRenderPass>(spec);
			case RenderApi::Api::Vulkan: return makeShare<Vulkan::VulkanRenderPass>(spec);
		}
		return nullptr;
	}

	RenderPass::~RenderPass() { }

}
