#pragma once
#include <vector>

#include "Engine/Core/Base.h"

#include "Shader.h"
#include "RenderPass.h"
#include "Descriptors.h"

namespace RT
{

	using PipelineLayouts = std::vector<DescriptorLayout*>;

	struct Pipeline
	{
		virtual void init(const Shader& shader, const RenderPass& renderPass, const PipelineLayouts& pipelineLayouts) = 0;
		virtual void initComp(const Shader& shader, const PipelineLayouts& pipelineLayouts) = 0;
		virtual void shutdown() = 0;

		virtual void bind() const = 0;
		virtual void dispatch(const glm::uvec2 groups) const = 0;

		static Local<Pipeline> create();
	};

}
