#pragma once
#include <vector>
#include <filesystem>

#include "Engine/Core/Base.h"

#include "RenderPass.h"
#include "Buffer.h"

namespace RT
{

	struct UniformLayout
	{
		uint32_t nrOfSets = 0u;
		std::vector<UniformType> layout = {};
	};

	using UniformLayouts = std::vector<UniformLayout>;

	struct PipelineSpec
	{
		std::filesystem::path shaderPath = ".";
		UniformLayouts uniformLayouts = {};
		AttachmentFormats attachmentFormats = {};
	};

	struct Pipeline
	{
		virtual ~Pipeline() = 0;

		virtual void updateSet(const uint32_t layout, const uint32_t set, const uint32_t binding, const Uniform& uniform) const = 0;
		virtual void bindSet(const uint32_t layout, const uint32_t set) const = 0;

		virtual void bind() const = 0;
		virtual void dispatch(const glm::uvec2 groups) const = 0;

		static Local<Pipeline> create(const PipelineSpec& spec);
	};

}
