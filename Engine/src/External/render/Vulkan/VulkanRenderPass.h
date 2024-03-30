#pragma once
#include <vector>

#include "Engine/Render/RenderPass.h"

#include "VulkanTexture.h"

#include <vulkan/vulkan.h>

namespace RT::Vulkan
{

	class Swapchain;

	class VulkanRenderPass : public RenderPass
	{
	public:
		VulkanRenderPass(const RenderPassSpec& spec);
		~VulkanRenderPass() final;

		void bind() const final;
		void unbind() const final;

		const Texture& getAttachment(const uint32_t idx = 0) const final
		{
			return attachments[idx];
		}

		const VkFramebuffer& getFrameBuffer(const uint32_t idx) const { return frameBuffers[idx]; }

	private:
		void createAttachments(const std::vector<ImageFormat>& attachmentTypes);
		void createRenderPass(const std::vector<ImageFormat>& attachmentTypes);
		void createFrameBuffers();

	private:
		VkRenderPass renderPass = {};
		glm::uvec2 size = {};

		std::vector<VulkanTexture> attachments = {};
		std::vector<VkFramebuffer> frameBuffers = {};
	};

}
