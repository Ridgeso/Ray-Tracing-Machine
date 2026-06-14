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
namespace
{
	void flushUniforms(const uint8_t slotIdx)
	{
		auto& uniformsToFlush = getUniformsToFlush();
		int32_t i = 0;
		int32_t flashedUniformsFrom = uniformsToFlush.size();
		while (i < flashedUniformsFrom)
		{
			if (uniformsToFlush[i]->flush(slotIdx))
			{
				i++;
				continue;
			}
			std::swap(uniformsToFlush[i], uniformsToFlush[--flashedUniformsFrom]);
		}
		uniformsToFlush.erase(uniformsToFlush.begin() + flashedUniformsFrom, uniformsToFlush.end());
	}
} // namespace

	VulkanRenderApi::VulkanRenderApi()
	{
		RT_ASSERT(checkValidationLayerSupport(), "validation layers requested, but not available!");
	}

	void VulkanRenderApi::init()
	{
		auto size = Application::getWindow()->getSize();
		extent = VkExtent2D{ (uint32_t)size.x, (uint32_t)size.y };

		auto& device = DeviceInstance;

		device.init();
		recreateSwapchain();

		initImGui();

		uiCmdBuffer = device.createCommandBuffer(device.getCommandPool());

		graphicsRecorder.init(device.getGraphicsQueue(), device.getCommandPool());
		computeRecorder.init(device.getComputeQueue(), device.getComputeCommandPool());

		Event::Event<Event::WindowResize>::registerCallback([this](const auto& event)
		{
			if (event.isMinimized)
			{
				return;
			}

			this->recreateSwapchain();
		});

		renderThread.start(uiCmdBuffer);
	}

	void VulkanRenderApi::shutdown()
	{
		renderThread.stop();

		auto& deviceInstance = DeviceInstance;
		
		deviceInstance.waitForIdle();

		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(deviceInstance.getDevice(), descriptorPool, nullptr);

		uiCmdBuffer.destroy();

		graphicsRecorder.shutdown();
		computeRecorder.shutdown();

		SwapchainInstance->shutdown();
		deviceInstance.shutdown();
	}

	void VulkanRenderApi::stop()
	{
		renderThread.stop();
		DeviceInstance.waitForIdle();
	}

	void VulkanRenderApi::waitForFrameReady()
	{
		renderThread.waitImGuiConsumed();
	}

	void VulkanRenderApi::beginFrame()
	{
		graphicsRecorder.begin();
	}

	void VulkanRenderApi::endFrame()
	{
		graphicsRecorder.end();
		flushUniforms(Context::slotIdx);
		pendingGraphicsSemaphore = graphicsRecorder.submit();
	}

	void VulkanRenderApi::submitUI()
	{
		currentSlot = renderThread.acquireFreeSlot();

		const auto slotIdx = currentSlot->slotIdx;
		SwapchainInstance->waitSlotFence(slotIdx);

		uiCmdBuffer.begin(slotIdx);
		uiCmdBuffer.end(slotIdx);

		flushUniforms(slotIdx);

		auto* slot = currentSlot;
		currentSlot = nullptr;

		const auto computeSem = pendingComputeSemaphore;
		pendingComputeSemaphore = VK_NULL_HANDLE;
		const auto graphicsSem = pendingGraphicsSemaphore;
		pendingGraphicsSemaphore = VK_NULL_HANDLE;

		renderThread.submitSlot(slot, computeSem, graphicsSem);
	}

	void VulkanRenderApi::beginCompute()
	{
		computeRecorder.begin();
	}

	void VulkanRenderApi::endCompute()
	{
		computeRecorder.end();
		flushUniforms(Context::slotIdx);
		pendingComputeSemaphore = computeRecorder.submit();
	}

	void VulkanRenderApi::recreateSwapchain()
	{
		auto size = Application::getWindow()->getSize();
		if (glm::ivec2{ 0, 0 } == size)
		{
			return;
		}

		renderThread.drainAndPause();
		DeviceInstance.waitForIdle();
		extent = VkExtent2D{ (uint32_t)size.x, (uint32_t)size.y };

		auto oldSwapchain = std::shared_ptr<Swapchain>(nullptr);
		if (SwapchainInstance == nullptr)
		{
			SwapchainInstance = std::make_unique<Swapchain>(extent);
		}
		else
		{
			// TODO: check rendepass compatibility
			oldSwapchain = std::shared_ptr<Swapchain>(SwapchainInstance.release());
			SwapchainInstance = std::make_unique<Swapchain>(extent, oldSwapchain);
		}

		SwapchainInstance->init();

		if (oldSwapchain)
		{
			RT_ASSERT(SwapchainInstance->compareFormats(*oldSwapchain), "swapchain image/depth formats has changed");
			oldSwapchain->shutdown();
		}
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
		vkInfo.QueueFamily = device.getQueueFamilyIndices().graphics->index;
		vkInfo.Queue = device.getImGuiQueue();
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

} // namespace RT::Vulkan

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
