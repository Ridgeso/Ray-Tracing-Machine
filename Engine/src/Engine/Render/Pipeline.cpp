#include "RenderApi.h"
#include "Pipeline.h"

#include "External/render/Vulkan/VulkanPipeline.h"

namespace RT
{

	Local<Pipeline> Pipeline::create(PipelineSpec& spec)
	{
		switch (RenderApi::api)
		{
			// case RenderApi::Api::OpenGL: return nullptr;
			case RenderApi::Api::Vulkan: return makeLocal<Vulkan::VulkanPipeline>(spec);
		}
		return nullptr;
	}

}
