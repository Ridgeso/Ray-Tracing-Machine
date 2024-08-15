#pragma once
#include <vector>

#include "Engine/Render/RenderApi.h"

#include "utils/Constants.h"

#include <vulkan/vulkan.h>

namespace RT::Vulkan
{

	class VulkanRenderApi final : public RenderApi
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

		void recreateSwapchain();

	private:
		void recordGuiCommandbuffer(const uint32_t imIdx);

		void initImGui();
		void allocateCmdBuffers(std::vector<VkCommandBuffer>& cmdBuff);
		void freeCmdBuffers(std::vector<VkCommandBuffer>& cmdBuff);

		static void flushUniforms();

	private:
		std::vector<VkCommandBuffer> cmdBuffers = {};
		std::vector<VkCommandBuffer> imGuiCmdBuffers = {};
		
		VkExtent2D extent = {};

		VkPipelineCache pipelineCache = {};
		VkDescriptorPool descriptorPool = {};
	};

}
