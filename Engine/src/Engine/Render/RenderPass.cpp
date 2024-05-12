#include "RenderBase.h"
#include "RenderPass.h"

#include "External/Render/OpenGl/OpenGlRenderPass.h"
#include "External/Render/Vulkan/VulkanRenderPass.h"

namespace RT
{

	Share<RenderPass> RenderPass::create(const RenderPassSpec& spec)
	{
		switch (GlobalRenderAPI)
		{
			// case RenderAPI::OpenGL: return makeShare<OpenGl::OpenGlRenderPass>(spec);
			case RenderAPI::Vulkan: return makeShare<Vulkan::VulkanRenderPass>(spec);
		}
		return nullptr;
	}

	RenderPass::~RenderPass() { }

}
