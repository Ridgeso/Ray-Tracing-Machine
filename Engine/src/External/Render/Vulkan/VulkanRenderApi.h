#pragma once
#include <array>

#include "Engine/Render/RenderApi.h"

#include "CommandBuffer.h"
#include "Fence.h"
#include "RenderThread.h"
#include "Semaphore.h"

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

		void beginCompute() final;
		void endCompute() final;

		void waitForFrameReady() final;

		void recreateSwapchain();

	private:
		void initImGui();
		void allocateCmdBuffers(CommandBuffers& cmdBuff, const VkCommandPool commandPool);
		void freeCmdBuffers(CommandBuffers& cmdBuff, const VkCommandPool commandPool);

	private:
		CommandBuffers cmdBuffers = {};
		CommandBuffers computeCmdBuffers = {};
		CommandBuffers imGuiCmdBuffers = {};

		FrameSlot* currentSlot = nullptr;

		RenderThread renderThread = {};

		Fences computeFences = {};
		Semaphores computeFinishedSemaphores = {};
		uint8_t computeSlotIdx = 0u;
		VkSemaphore pendingComputeSemaphore = VK_NULL_HANDLE;

		VkExtent2D extent = {};

		VkPipelineCache pipelineCache = {};
		VkDescriptorPool descriptorPool = {};
	};

}
