#include "RenderBase.h"

#include "Pipeline.h"

#include "External/render/Vulkan/VulkanPipeline.h"

namespace RT
{

	Local<Pipeline> Pipeline::create(const PipelineSpec& spec)
	{
		switch (GlobalRenderAPI)
		{
			// case RT::RenderAPI::OpenGL: return nullptr;
			case RT::RenderAPI::Vulkan: return makeLocal<Vulkan::VulkanPipeline>(spec);
		}
		return nullptr;
	}

	Pipeline::~Pipeline() {}

}
