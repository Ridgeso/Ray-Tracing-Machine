#pragma once
#include <array>

#include "Engine/Render/RenderApi.h"

#include "CommandBuffer.h"
#include "Fence.h"
#include "RenderThread.h"
#include "Semaphore.h"
#include "External/Render/Vulkan/rendering/RenderRecorder.h"

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

		bool beginCompute() final;
		void endCompute() final;

		void submitUI() final;

		void waitForFrameReady() final;

		void recreateSwapchain();

	private:
		void initImGui();

	private:
		CommandBuffer uiCmdBuffer = {};

		FrameSlot* currentSlot = nullptr;

		RenderThread renderThread = {};

		RenderRecorder graphicsRecorder = {};
		RenderRecorder computeRecorder = {};
		VkSemaphore pendingGraphicsSemaphore = VK_NULL_HANDLE;
		VkSemaphore pendingComputeSemaphore = VK_NULL_HANDLE;

		VkExtent2D extent = {};

		VkPipelineCache pipelineCache = {};
		VkDescriptorPool descriptorPool = {};
	};

}
