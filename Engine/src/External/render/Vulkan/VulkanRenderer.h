#pragma once
#include "Engine/Core/Base.h"
#include "Engine/Render/Renderer.h"

#include <vulkan/vulkan.h>
#include "Device.h"
#include "Swapchain.h"
#include "VulkanPipeline.h"
#include "VulkanBuffer.h"
#include "VulkanDescriptors.h"
#include "VulkanRenderPass.h"

namespace RT::Vulkan
{

	class VulkanRenderer : public Renderer
	{
	public:
		VulkanRenderer();
		~VulkanRenderer() = default;

		VulkanRenderer(const VulkanRenderer&) = delete;
		VulkanRenderer(VulkanRenderer&&) = delete;
		VulkanRenderer& operator=(const VulkanRenderer&) = delete;
		VulkanRenderer&& operator=(VulkanRenderer&&) = delete;

		void init(const RenderSpecs& specs) final;
		void shutDown() final;
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
