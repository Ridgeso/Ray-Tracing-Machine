#pragma once
#include <vector>

#include "Engine/Render/RenderApi.h"

#include <vulkan/vulkan.h>

namespace RT::Vulkan
{

	class VulkanRenderApi : public RenderApi
	{
	public:
		VulkanRenderApi();
		~VulkanRenderApi() = default;

		VulkanRenderApi(const VulkanRenderApi&) = delete;
		VulkanRenderApi(VulkanRenderApi&&) = delete;
		VulkanRenderApi& operator=(const VulkanRenderApi&) = delete;
		VulkanRenderApi&& operator=(VulkanRenderApi&&) = delete;

		void init() final;
		void shutdown() final;
		void stop() final;

		void beginFrame() final;
		void endFrame() final;

	private:
		void recordCommandbuffer(const uint32_t imIdx);
		void recreateSwapchain();

		void initImGui();

	private:
		std::vector<VkCommandBuffer> commandBuffers = {};
		
		VkExtent2D extent = {};

		VkPipelineCache pipelineCache = {};
		VkDescriptorPool descriptorPool = {};
	};

}
