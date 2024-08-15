#include "VulkanRenderApi.h"
#include "utils/Debug.h"
#include "Context.h"
#include "Device.h"
#include "Swapchain.h"
#include "VulkanBuffer.h"

#include "Engine/Core/Application.h"

#include "Engine/Event/Event.h"
#include "Engine/Event/AppEvents.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>

namespace RT::Vulkan
{

	VulkanRenderApi::VulkanRenderApi()
	{
		RT_ASSERT(checkValidationLayerSupport(), "validation layers requested, but not available!");
	}

	void VulkanRenderApi::init()
	{
		auto size = Application::getWindow()->getSize();
		extent = VkExtent2D{ (uint32_t)size.x, (uint32_t)size.y };

		DeviceInstance.init();
		recreateSwapchain();
		
		initImGui();

		allocateCmdBuffers(cmdBuffers);
		allocateCmdBuffers(imGuiCmdBuffers);

		Event::Event<Event::WindowResize>::registerCallback([this](const auto& event)
		{
			if (event.isMinimized)
			{
				return;
			}

			this->recreateSwapchain();
		});
	}

	void VulkanRenderApi::shutdown()
	{
		auto& deviceInstance = DeviceInstance;
		vkDeviceWaitIdle(deviceInstance.getDevice());

		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(deviceInstance.getDevice(), descriptorPool, nullptr);

		freeCmdBuffers(cmdBuffers);
		freeCmdBuffers(imGuiCmdBuffers);
		
		SwapchainInstance->shutdown();
		deviceInstance.shutdown();
	}

	void VulkanRenderApi::stop()
	{
		vkDeviceWaitIdle(DeviceInstance.getDevice());
	}

	void VulkanRenderApi::beginFrame()
	{
		uint32_t imgIdx = 0u;
		auto result = SwapchainInstance->acquireNextImage(imgIdx);

		flushUniforms();

		// Probably not needed as it is handled by WindowResize event callback, but keept for safty
		if (VK_ERROR_OUT_OF_DATE_KHR == result)
		{
			recreateSwapchain();
			result = SwapchainInstance->acquireNextImage(imgIdx);
		}

		RT_ASSERT(VK_SUCCESS == result || VK_SUBOPTIMAL_KHR == result, "failte to acquire swap chain image!");

		Context::imgIdx = imgIdx;
		Context::frameCmd = cmdBuffers[imgIdx];

		auto beginInfo = VkCommandBufferBeginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		vkResetCommandBuffer(Context::frameCmd, 0);
		CHECK_VK(vkBeginCommandBuffer(Context::frameCmd, &beginInfo), "failed to begin command buffer!");
	}

	void VulkanRenderApi::endFrame()
	{
		CHECK_VK(vkEndCommandBuffer(Context::frameCmd), "failed to record command buffer");

		recordGuiCommandbuffer(Context::imgIdx);

		auto result = SwapchainInstance->submitCommandBuffers(cmdBuffers[Context::imgIdx], imGuiCmdBuffers[Context::imgIdx], Context::imgIdx);

		// Probably not needed as it is handled by WindowResize event callback, but keept for safty
		if (VK_ERROR_OUT_OF_DATE_KHR == result || VK_SUBOPTIMAL_KHR == result)
		{
			recreateSwapchain();
			return;
		}

		RT_ASSERT(VK_SUCCESS == result, "failte to present swap chain image!");

		Context::imgIdx = invalidImgIdx;
		Context::frameCmd = VK_NULL_HANDLE;
	}

	void VulkanRenderApi::recreateSwapchain()
	{
		auto size = Application::getWindow()->getSize();
		if (glm::ivec2{ 0, 0 } == size)
		{
			return;
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
			RT_ASSERT(SwapchainInstance->compareFormats(*oldSwapchain), "swapchain image/depth formats has changed");
			oldSwapchain->shutdown();
		}
	}

	void VulkanRenderApi::recordGuiCommandbuffer(const uint32_t imIdx)
	{
		auto currCmdBuff = imGuiCmdBuffers[imIdx];

		auto beginInfo = VkCommandBufferBeginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkResetCommandBuffer(currCmdBuff, 0);
		CHECK_VK(vkBeginCommandBuffer(currCmdBuff, &beginInfo), "failed to begin command buffer!");

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

		vkCmdBeginRenderPass(currCmdBuff, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

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
		vkCmdSetViewport(currCmdBuff, 0, 1, &viewport);
		vkCmdSetScissor(currCmdBuff, 0, 1, &scissor);

		auto* drawData = ImGui::GetDrawData();
		ImGui_ImplVulkan_RenderDrawData(drawData, currCmdBuff);

		vkCmdEndRenderPass(currCmdBuff);

		CHECK_VK(vkEndCommandBuffer(currCmdBuff), "failed to record command buffer");
	}

	void VulkanRenderApi::initImGui()
	{
		auto& device = DeviceInstance;

		constexpr auto poolSizes = std::array{
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

		CHECK_VK(
			vkCreateDescriptorPool(device.getDevice(), &poolInfo, nullptr, &descriptorPool),
			"failed to create descriptor pool!");

		auto result = ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)Application::getWindow()->getNativWindow(), true);
		RT_ASSERT(result, "ImGui not implemented");

		// init ImGui for vulkan
		auto vkInfo = ImGui_ImplVulkan_InitInfo{};
		vkInfo.Instance = device.getInstance();
		vkInfo.PhysicalDevice = device.getPhysicalDevice();
		vkInfo.Device = device.getDevice();
		vkInfo.QueueFamily = device.getQueueFamilyIndices().graphicsFamily;
		vkInfo.Queue = device.getGraphicsQueue();
		vkInfo.PipelineCache = pipelineCache;
		vkInfo.DescriptorPool = descriptorPool;
		vkInfo.RenderPass = SwapchainInstance->getRenderPass();
		vkInfo.Subpass = 0;
		vkInfo.MinImageCount = Swapchain::minImageCount();
		vkInfo.ImageCount = SwapchainInstance->getSwapChainImages().size();
		vkInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		vkInfo.Allocator = nullptr;
		vkInfo.CheckVkResultFn = checkVkResultCallback;
		result = ImGui_ImplVulkan_Init(&vkInfo);
		RT_ASSERT(result, "ImGui not initialized");
	}

	void VulkanRenderApi::allocateCmdBuffers(std::vector<VkCommandBuffer>& cmdBuff)
	{
		cmdBuff.resize(SwapchainInstance->getSwapChainImages().size());

		auto allocInfo = VkCommandBufferAllocateInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = DeviceInstance.getCommandPool();
		allocInfo.commandBufferCount = static_cast<uint32_t>(cmdBuff.size());

		CHECK_VK(
			vkAllocateCommandBuffers(DeviceInstance.getDevice(), &allocInfo, cmdBuff.data()),
			"failed to allocate command buffers!");
	}

	void VulkanRenderApi::freeCmdBuffers(std::vector<VkCommandBuffer>& cmdBuff)
	{
		vkFreeCommandBuffers(
			DeviceInstance.getDevice(),
			DeviceInstance.getCommandPool(),
			static_cast<uint32_t>(cmdBuff.size()),
			cmdBuff.data());
		cmdBuff.clear();
	}

	void VulkanRenderApi::flushUniforms()
	{
		auto& uniformsToFlush = getUniformsToFlush();
		int32_t i = 0;
		int32_t flashedUniformsFrom = uniformsToFlush.size();
		while (i < uniformsToFlush.size())
		{
			if (not uniformsToFlush[i]->flush())
			{
				std::swap(uniformsToFlush[i], uniformsToFlush[--flashedUniformsFrom]);
			}
			else
			{
				i++;
			}
		}
		uniformsToFlush.erase(uniformsToFlush.begin() + flashedUniformsFrom, uniformsToFlush.end());
	}

}

///////////////////////////// Just a reminder for post processing /////////////////////////////

//const auto rpExtent = VkExtent2D(renderPass.getSize().x, renderPass.getSize().y);

//VkRenderPassBeginInfo renderPassInfo{};
//renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
//
//renderPassInfo.renderPass = renderPass.getRenderPass();
//renderPassInfo.framebuffer = renderPass.getFrameBuffer(0);
//
//renderPassInfo.renderArea.offset = { 0, 0 };
//renderPassInfo.renderArea.extent = rpExtent;
//
//constexpr auto clearValues = std::array<VkClearValue, 3>{
//	VkClearValue{ { 0.1f, 0.1f, 0.1f, 1.0f } }, // color
//	VkClearValue{ { 0.1f, 0.1f, 0.1f, 1.0f } }, // color
//	VkClearValue{ { 1.0f, 0 } } // depthStencil
//};
//renderPassInfo.clearValueCount = clearValues.size();
//renderPassInfo.pClearValues = clearValues.data();
//
//vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

//auto viewport = VkViewport{};
//viewport.x = 0;
//viewport.y = 0;
//viewport.width = (float)rpExtent.width;
//viewport.height = (float)rpExtent.height;
//viewport.minDepth = 0.0f;
//viewport.maxDepth = 1.0f;
//auto scissor = VkRect2D{};
//scissor.offset = { 0, 0 };
//scissor.extent = rpExtent;
//vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
//vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

//vkPipeline.bind(cmdBuffer);
//vkVbuffer.bind(cmdBuffer);
//vkVbuffer.draw(cmdBuffer);

//vkCmdEndRenderPass(cmdBuffer);

//static_cast<const VulkanTexture&>(renderPass.getAttachment(1)).transition(
//	VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
//	VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//	VK_ACCESS_TRANSFER_WRITE_BIT,
//	VK_ACCESS_SHADER_READ_BIT,
//	cmdBuffer);
