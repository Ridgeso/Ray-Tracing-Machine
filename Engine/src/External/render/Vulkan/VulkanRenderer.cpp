#include "VulkanRenderer.h"

#include "Engine/Core/Assert.h"
#include "Engine/Core/Application.h"

#include "utils/Debug.h"
#include "Engine/Core/Assert.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <GLFW/glfw3.h>

#include "GLFW/glfw3.h"

namespace RT::Vulkan
{

	VulkanRenderer::VulkanRenderer()
	{
		if (EnableValidationLayers)
		{
			RT_CORE_ASSERT(checkValidationLayerSupport(), "validation layers requested, but not available!");
		}
	}

	void VulkanRenderer::init(const RenderSpecs& specs)
	{
		auto& window = *Application::Get().getWindow();
		auto size = window.getSize();
		extent = VkExtent2D{ (uint32_t)size.x, (uint32_t)size.y };

		auto& deviceInstance = DeviceInstance;

		pipeline = std::make_unique<Pipeline>();
		deviceInstance.init(window);
		recreateSwapchain();
		
		initImGui();

		auto triangleVert = std::vector<Vertex>{
			{{0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
			{{0.5f, -0.5f},  {0.0f, 1.0f, 0.0f}},
			{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
			{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
			{{-0.5f, 0.5f},  {0.0f, 0.0f, 1.0f}},
			{{-0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}}
		};
		vertexBuffer = makeLocal<VulkanVertexBuffer>(
			static_cast<uint32_t>(triangleVert.size()),
			triangleVert.data());

		auto pipelineLayoutInfo = VkPipelineLayoutCreateInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pSetLayouts = nullptr;
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;

		RT_CORE_ASSERT(
			vkCreatePipelineLayout(deviceInstance.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS,
			"Could not create pipeline Layout");

		commandBuffers.resize(SwapchainInstance->getSwapChainImages().size());

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = deviceInstance.getCommandPool();
		allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

		RT_CORE_ASSERT(
			vkAllocateCommandBuffers(deviceInstance.getDevice(), &allocInfo, commandBuffers.data()) == VK_SUCCESS,
			"failed to allocate command buffers!");
	}

	void VulkanRenderer::shutDown()
	{
		auto& deviceInstance = DeviceInstance;
		vkDeviceWaitIdle(deviceInstance.getDevice());

		vkDestroyDescriptorPool(deviceInstance.getDevice(), descriptorPool, nullptr);
		ImGui_ImplVulkan_Shutdown();

		vkDestroyPipelineLayout(deviceInstance.getDevice(), pipelineLayout, nullptr);
		vkFreeCommandBuffers(
			deviceInstance.getDevice(),
			deviceInstance.getCommandPool(),
			static_cast<uint32_t>(commandBuffers.size()),
			commandBuffers.data());
		commandBuffers.clear();

		pipeline->shutdown();
		
		vertexBuffer.reset();

		SwapchainInstance->shutdown();
		deviceInstance.shutdown();
	}

	void VulkanRenderer::render(
		const RenderPass& renderPass,
		const Camera& camera,
		const Shader& shader,
		const VertexBuffer& vbuffer,
		const Scene& scene)
	{
		const auto& vkRenderPass = static_cast<const VulkanRenderPass&>(renderPass);
		if (pipeline->getGraphicsPipeline() == VK_NULL_HANDLE)
		{
			createPipeline(vkRenderPass.getRenderPass());
		}

		uint32_t imageIndex = 0u;
		auto result = SwapchainInstance->acquireNextImage(imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreateSwapchain();
		}

		RT_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR, "failte to acquire swap chain image!");
		
		recordCommandbuffer(imageIndex, vkRenderPass);

		result = SwapchainInstance->submitCommandBuffers(commandBuffers[imageIndex], imageIndex);
		
		if (VK_ERROR_OUT_OF_DATE_KHR == result || VK_SUBOPTIMAL_KHR == result)
		{
			recreateSwapchain();
			return;
		}

		RT_ASSERT(result == VK_SUCCESS, "failte to present swap chain image!");
	}

	void VulkanRenderer::recordCommandbuffer(const uint32_t imIdx, const VulkanRenderPass& renderPass)
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		RT_CORE_ASSERT(vkBeginCommandBuffer(commandBuffers[imIdx], &beginInfo) == VK_SUCCESS, "failed to begin command buffer!");

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		
		renderPassInfo.renderPass = SwapchainInstance->getRenderPass();
		renderPassInfo.framebuffer = SwapchainInstance->getFramebuffers()[imIdx];

		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = SwapchainInstance->getWindowExtent();

		constexpr auto clearValues = std::array<VkClearValue, 1>{
			VkClearValue{ { 0.1f, 0.1f, 0.1f, 1.0f } }, // color
		};
		renderPassInfo.clearValueCount = clearValues.size();
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(commandBuffers[imIdx], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		auto viewport = VkViewport{};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = SwapchainInstance->getSwapchainExtent().width;
		viewport.height = SwapchainInstance->getSwapchainExtent().height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		auto scissor = VkRect2D{};
		scissor.offset = { 0, 0 };
		scissor.extent = SwapchainInstance->getSwapchainExtent();
		vkCmdSetViewport(commandBuffers[imIdx], 0, 1, &viewport);
		vkCmdSetScissor(commandBuffers[imIdx], 0, 1, &scissor);

		auto* drawData = ImGui::GetDrawData();
		if (drawData != nullptr)
		{
			ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffers[imIdx]);
		}

		vkCmdEndRenderPass(commandBuffers[imIdx]);

		registerFrameBuff(commandBuffers[imIdx], renderPass);

		RT_CORE_ASSERT(vkEndCommandBuffer(commandBuffers[imIdx]) == VK_SUCCESS, "failed to record command buffer");
	}

	void VulkanRenderer::recreateSwapchain()
	{
		auto& window = *Application::Get().getWindow();
		auto size = window.getSize();
		while (size.x == 0 || size.y == 0)
		{
			size = window.getSize();
			glfwWaitEvents();
		}
		vkDeviceWaitIdle(DeviceInstance.getDevice());
		extent = VkExtent2D{ (uint32_t)size.x, (uint32_t)size.y };

		auto oldSwapchain = Share<Swapchain>(nullptr);
		if (SwapchainInstance == nullptr)
		{
			SwapchainInstance = makeLocal<Swapchain>(extent);
		}
		else
		{
			// TODO: check rendepass compatibility
			oldSwapchain = Share<Swapchain>(SwapchainInstance.release());
			SwapchainInstance = makeLocal<Swapchain>(extent, oldSwapchain);
		}
		
		SwapchainInstance->init();

		if (oldSwapchain)
		{
			RT_CORE_ASSERT(SwapchainInstance->compareFormats(*oldSwapchain), "swapchain image/depth formats has changed");
			oldSwapchain->shutdown();
		}
	}

	void VulkanRenderer::initImGui()
	{
		Vulkan::Device& device = DeviceInstance;

		constexpr auto poolSizes = std::array<VkDescriptorPoolSize, 11>{
			VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLER, 100 },
			VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
			VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100 },
			VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 },
			VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100 },
			VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100 },
			VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
			VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },
			VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100 },
			VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100 },
			VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100 }
		};

		auto poolInfo = VkDescriptorPoolCreateInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		poolInfo.maxSets = 1000 * poolSizes.size();
		poolInfo.poolSizeCount = poolSizes.size();
		poolInfo.pPoolSizes = poolSizes.data();

		RT_CORE_ASSERT(
			vkCreateDescriptorPool(device.getDevice(), &poolInfo, nullptr, &descriptorPool) == VK_SUCCESS,
			"failed to create descriptor pool!");

		auto& window = *Application::Get().getWindow();
		RT_ASSERT(
			ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)window.getNativWindow(), true),
			"ImGui not implemented");

		// init ImGui for vulkan
		auto vkInfo = ImGui_ImplVulkan_InitInfo{};
		vkInfo.Instance = device.getInstance();
		vkInfo.PhysicalDevice = device.getPhysicalDevice();
		vkInfo.Device = device.getDevice();
		vkInfo.QueueFamily = device.getQueueFamilyIndices().graphicsFamily;
		vkInfo.Queue = device.getGraphicsQueue();
		vkInfo.PipelineCache = pipelineCache;
		vkInfo.DescriptorPool = descriptorPool;
		vkInfo.Subpass = 0;
		vkInfo.MinImageCount = Swapchain::minImageCount();
		vkInfo.ImageCount = SwapchainInstance->getSwapChainImages().size();
		vkInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		vkInfo.Allocator = nullptr;
		vkInfo.CheckVkResultFn = checkVkResultCallback;
		RT_ASSERT(
			ImGui_ImplVulkan_Init(&vkInfo, SwapchainInstance->getRenderPass()),
			"ImGui not initialized");

		// upload fonst
		device.execSingleCmdPass([](const auto cmdBuff)
		{
			ImGui_ImplVulkan_CreateFontsTexture(cmdBuff);
		});
		vkDeviceWaitIdle(device.getDevice());
		ImGui_ImplVulkan_DestroyFontUploadObjects();	
	}

	void VulkanRenderer::registerFrameBuff(VkCommandBuffer cmdBuffer, const VulkanRenderPass& renderPass)
	{
		const auto rpExtent = VkExtent2D(renderPass.getSize().x, renderPass.getSize().y);

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		
		renderPassInfo.renderPass = renderPass.getRenderPass();
		renderPassInfo.framebuffer = renderPass.getFrameBuffer(0);
		
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = rpExtent;
		
		constexpr auto clearValues = std::array<VkClearValue, 3>{
			VkClearValue{ { 0.1f, 0.1f, 0.1f, 1.0f } }, // color
			VkClearValue{ { 0.1f, 0.1f, 0.1f, 1.0f } }, // color
			VkClearValue{ { 1.0f, 0 } } // depthStencil
		};
		renderPassInfo.clearValueCount = clearValues.size();
		renderPassInfo.pClearValues = clearValues.data();
		
		vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		
		auto viewport = VkViewport{};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = (float)rpExtent.width;
		viewport.height = (float)rpExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		auto scissor = VkRect2D{};
		scissor.offset = { 0, 0 };
		scissor.extent = rpExtent;
		vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
		
		pipeline->bind(cmdBuffer);
		vertexBuffer->bind(cmdBuffer);
		vertexBuffer->draw(cmdBuffer);
		
		vkCmdEndRenderPass(cmdBuffer);
		
		static_cast<const VulkanTexture&>(renderPass.getAttachment(1)).transition(
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			cmdBuffer);
	}

	void VulkanRenderer::createPipeline(const VkRenderPass rp)
	{
		auto pipelineConfig = PipelineConfigInfo{};
		Pipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.renderPass = rp;
		pipelineConfig.pipelineLayout = pipelineLayout;

		pipeline->init("..\\Engine\\assets\\shaders\\triangle.shader", pipelineConfig);
	}

}
