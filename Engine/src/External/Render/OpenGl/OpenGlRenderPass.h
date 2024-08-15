#pragma once
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

#include "Engine/Render/RenderPass.h"
#include "OpenGlTexture.h"

namespace RT::OpenGl
{

	class OpenGlRenderPass // : public RenderPass
	{
	public:
		OpenGlRenderPass(const RenderPassSpec& spec);
		~OpenGlRenderPass(); // final;

		const Texture& getAttachment(const uint32_t index = 0) const; // final;

		void bind() const;
		void unbind() const;

	private:
		uint32_t renderId;
		uint32_t frameId;
		glm::uvec2 size;
		std::vector<OpenGlTexture> attachments;
	};

}
