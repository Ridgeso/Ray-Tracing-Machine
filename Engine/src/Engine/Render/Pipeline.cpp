#include "Pipeline.h"
#include "RenderApi.h"

#include "External/render/Vulkan/VulkanPipeline.h"

namespace RT
{

	std::unique_ptr<Pipeline> Pipeline::create(PipelineSpec& spec)
	{
		switch (RenderApi::api)
		{
			// case RenderApi::Api::OpenGL: return nullptr;
			case RenderApi::Api::Vulkan: return std::make_unique<Vulkan::VulkanPipeline>(spec);
		}
		return nullptr;
	}

}
