#pragma once
#include "Engine/Core/Base.h"
#include "Engine/Render/Renderer.h"

#include <vulkan/vulkan.h>
#include "Device.h"
#include "Swapchain.h"
#include "Pipeline.h"
#include "VulkanBuffer.h"
#include "Descriptors.h"
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

		void render(
			const RenderPass& renderPass,
			const Camera& camera,
			const Shader& shader,
			const VertexBuffer& vbuffer,
			const Scene& scene) final;
	
	private:
		void recordCommandbuffer(const uint32_t imIdx, const VulkanRenderPass& renderPass);
		void recreateSwapchain();

		void initImGui();

		void registerFrameBuff(VkCommandBuffer cmdBuffer, const VulkanRenderPass& renderPass);
		void createPipeline(const VkRenderPass rp);

	private:
		VkPipelineLayout pipelineLayout = {};
		std::vector<VkCommandBuffer> commandBuffers = {};
		Local<Pipeline> pipeline = nullptr;
		
		Local<VulkanVertexBuffer> vertexBuffer = {};
		Local<VulkanUniform> uniform[2] = {};
		Local<VulkanStorage> storage[2] = {};
		Local<Descriptor> descriptor = {};

		VkExtent2D extent = {};

		VkPipelineCache pipelineCache = {};
		VkDescriptorPool descriptorPool = {};
	};

}
