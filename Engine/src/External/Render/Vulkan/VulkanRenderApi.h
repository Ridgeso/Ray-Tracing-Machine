#pragma once
#include <array>

#include "Engine/Render/RenderApi.h"

#include "RenderThread.h"
#include "utils/Constants.h"

#include <vulkan/vulkan.h>

namespace RT::Vulkan
{

	class VulkanRenderApi final : public RenderApi
	{
		using CommandBuffers = std::array<VkCommandBuffer, Constants::MAX_FRAMES_IN_FLIGHT>;

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

		void waitForFrameReady();

		void recreateSwapchain();

	private:
		void initImGui();
		void allocateCmdBuffers(CommandBuffers& cmdBuff);
		void freeCmdBuffers(CommandBuffers& cmdBuff);

	private:
		CommandBuffers cmdBuffers = {};
		CommandBuffers imGuiCmdBuffers = {};

		FrameSlot* currentSlot = nullptr;

		RenderThread renderThread = {};

		VkExtent2D extent = {};

		VkPipelineCache pipelineCache = {};
		VkDescriptorPool descriptorPool = {};
	};

}
