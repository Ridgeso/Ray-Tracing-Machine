#pragma once
#include <vector>
#include <Engine/Core/Base.h>
#include <glm/glm.hpp>

#include "Engine/Render/Texture.h"

namespace RT
{

	using AttachmentFormats = std::vector<Texture::Format>;

	struct RenderPassSpec
	{
		glm::uvec2 size = {};
		AttachmentFormats attachmentsFormats = {};
	};

	struct RenderPass
	{
		virtual ~RenderPass() = 0 {}

		virtual const Texture& getAttachment(const uint32_t idx = 0) const = 0;

		static Share<RenderPass> create(const RenderPassSpec& spec);
	};

}
