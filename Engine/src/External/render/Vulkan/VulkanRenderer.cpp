#include "External/render/Vulkan/VulkanRenderer.h"

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

	struct ImGuiFrame
	{
		VkCommandPool       CommandPool;
		VkCommandBuffer     CommandBuffer;
		VkFence             Fence;
		VkImage             Backbuffer;
		VkImageView         BackbufferView;
		VkFramebuffer       Framebuffer;
	};
	struct ImGuiFrameSemaphores
	{
		VkSemaphore         ImageAcquiredSemaphore;
		VkSemaphore         RenderCompleteSemaphore;
	};
	uint32_t currentFrame = 0;
	uint32_t frameIndex = 0;
	uint32_t semaphoreIndex = 0;
	uint32_t minImageCount = 2;
	uint32_t imageCount = 3;
	VkPipelineCache pipelineCache{};
	VkRenderPass igRenderPass{};
	VkDescriptorPool descriptorPool;
	ImGuiFrame* frames = nullptr;
	ImGuiFrameSemaphores* semaphores = nullptr;

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
		
		auto pipelineConfig = PipelineConfigInfo{};
		Pipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.renderPass = SwapchainInstance->getRenderPass();
		pipelineConfig.pipelineLayout = pipelineLayout;
		
		pipeline->init(
			"..\\Engine\\assets\\shaders\\vert.vert.spv",
			"..\\Engine\\assets\\shaders\\frag.frag.spv",
			pipelineConfig);

		commandBuffers.resize(SwapchainInstance->getSwapChainImages().size());

		//createImGuiRenderPass(); // Will be deleted soon
		initImGui();

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
		if (frames != nullptr)
		{
			for (uint32_t i = 0; i < imageCount; i++)
			{
				vkFreeCommandBuffers(deviceInstance.getDevice(), frames[i].CommandPool, 1, &frames[i].CommandBuffer);
				vkDestroyCommandPool(deviceInstance.getDevice(), frames[i].CommandPool, nullptr);
				vkDestroyFence(deviceInstance.getDevice(), frames[i].Fence, nullptr);
				vkDestroySemaphore(deviceInstance.getDevice(), semaphores[i].ImageAcquiredSemaphore, nullptr);
				vkDestroySemaphore(deviceInstance.getDevice(), semaphores[i].RenderCompleteSemaphore, nullptr);
			}

			for (int32_t i = 0; i < imageCount; i++)
			{
				vkDestroyImageView(deviceInstance.getDevice(), frames[i].BackbufferView, nullptr);
				vkDestroyFramebuffer(deviceInstance.getDevice(), frames[i].Framebuffer, nullptr);
			}
			delete frames;
			delete semaphores;
			vkDestroyRenderPass(deviceInstance.getDevice(), igRenderPass, nullptr);
		}

		ImGui_ImplVulkan_Shutdown();
		//ImGui_ImplGlfw_Shutdown();
		//ImGui::DestroyContext();

		///////
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

	void VulkanRenderer::render(const Camera& camera, const Shader& shader, const VertexBuffer& vbuffer, const Scene& scene)
	{
		uint32_t imageIndex = 0u;
		auto result = SwapchainInstance->acquireNextImage(imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreateSwapchain();
		}

		RT_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR, "failte to acquire swap chain image!");
		
		recordCommandbuffer(imageIndex);
		//drawImGui();

		result = SwapchainInstance->submitCommandBuffers(commandBuffers[imageIndex], imageIndex);
		
		if (VK_ERROR_OUT_OF_DATE_KHR == result || VK_SUBOPTIMAL_KHR == result)
		{
			recreateSwapchain();
			return;
		}

		RT_ASSERT(result == VK_SUCCESS, "failte to present swap chain image!");
	}

	void VulkanRenderer::recordCommandbuffer(const uint32_t imIdx)
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		RT_CORE_ASSERT(vkBeginCommandBuffer(commandBuffers[imIdx], &beginInfo) == VK_SUCCESS, "failed to begin command buffer!");

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = SwapchainInstance->getRenderPass();
		renderPassInfo.framebuffer = SwapchainInstance->getFramebuffers()[imIdx];
		//renderPassInfo.renderPass = igRenderPass;
		//renderPassInfo.framebuffer = frames[imIdx].Framebuffer;

		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = SwapchainInstance->getWindowExtent();

		constexpr auto clearValues = std::array<VkClearValue, 1>{
			VkClearValue{ { 0.1f, 0.1f, 0.1f, 1.0f } }, // color
			//VkClearValue{ { 1.0f, 0 } } // depthStencil
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

		//pipeline->bind(commandBuffers[imIdx]);
		//vertexBuffer->bind(commandBuffers[imIdx]);
		//vertexBuffer->draw(commandBuffers[imIdx]);

		// draw imgui
		auto* drawData = ImGui::GetDrawData();
		if (drawData != nullptr)
		{
			ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffers[imIdx]);
		}
		// end draw imgui

		vkCmdEndRenderPass(commandBuffers[imIdx]);
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

	void VulkanRenderer::drawImGui()
	{
		bool show_demo_window = true, show_another_window = false;
		static float f = 0.0f;
		static int counter = 0;

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		{
			ImGui::ShowDemoWindow(&show_demo_window);
			ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.
			ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
			ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
			ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
			if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
				counter++;
			ImGui::SameLine();
			ImGui::Text("counter = %d", counter);
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::End();
		}

		ImGui::Render();
		const ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}

	void VulkanRenderer::initImGui()
	{
		Vulkan::Device& device = DeviceInstance;

		// create DescriptorPool
		{
			constexpr auto poolSizes = std::array<VkDescriptorPoolSize, 11>{
				VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
				VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
				VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
				VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
				VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
				VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
				VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
				VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
				VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
				VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
				VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
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
		}

		// TODO: need to enable ImGui Viewports
		// init ImGui
		//IMGUI_CHECKVERSION();
		//ImGui::CreateContext();
		//ImGuiIO& io = ImGui::GetIO(); (void)io;
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard
		//	| ImGuiConfigFlags_DockingEnable
		//	| ImGuiConfigFlags_ViewportsEnable;
		//ImGui::StyleColorsClassic();
		//ImGuiStyle& style = ImGui::GetStyle();
		//if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		//{
		//	style.WindowRounding = 0.0f;
		//	style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		//}

		// Init ImGui instance
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
		//RT_ASSERT(
		//	ImGui_ImplVulkan_Init(&vkInfo, igRenderPass),
		//	"ImGui not initialized");

		// upload fonst
		auto allocInfo = VkCommandBufferAllocateInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = device.getCommandPool();
		allocInfo.commandBufferCount = 1;
		auto commandBuffer = VkCommandBuffer{};
		vkAllocateCommandBuffers(device.getDevice(), &allocInfo, &commandBuffer);
		auto beginInfo = VkCommandBufferBeginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(commandBuffer, &beginInfo);
		
		ImGui_ImplVulkan_CreateFontsTexture(commandBuffer); // fonts
		
		vkEndCommandBuffer(commandBuffer);
		auto submitInfo = VkSubmitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		vkQueueSubmit(device.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(device.getGraphicsQueue());
		vkFreeCommandBuffers(device.getDevice(), device.getCommandPool(), 1, &commandBuffer);
		
		vkDeviceWaitIdle(device.getDevice());
		ImGui_ImplVulkan_DestroyFontUploadObjects();	
	}

	void VulkanRenderer::createImGuiRenderPass()
	{
		Vulkan::Device& device = DeviceInstance;
		imageCount = SwapchainInstance->getSwapChainImages().size();

		frames = new ImGuiFrame[imageCount];
		semaphores = new ImGuiFrameSemaphores[imageCount];
		{
			memset(frames, 0, sizeof(ImGuiFrame) * imageCount);
			memset(semaphores, 0, sizeof(ImGuiFrameSemaphores) * imageCount);
			VkImage backbuffers[16] = {};
			vkGetSwapchainImagesKHR(device.getDevice(), SwapchainInstance->getSwapChain(), &imageCount, backbuffers);
			for (uint32_t i = 0; i < imageCount; i++)
				frames[i].Backbuffer = backbuffers[i];
		}

		auto attachment = VkAttachmentDescription{};
		attachment.format = SwapchainInstance->getImageFormat();
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		auto colorAttachment = VkAttachmentReference{};
		colorAttachment.attachment = 0;
		colorAttachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		auto subpass = VkSubpassDescription{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachment;

		auto dependency = VkSubpassDependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;  // or VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		auto renderPassInfo = VkRenderPassCreateInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &attachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;
		RT_CORE_ASSERT(
			vkCreateRenderPass(DeviceInstance.getDevice(), &renderPassInfo, nullptr, &igRenderPass) == VK_SUCCESS,
			"Could not create Dear ImGui's render pass");
		
		VkImageSubresourceRange imageRange = {};
		imageRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageRange.baseMipLevel = 0;
		imageRange.levelCount = 1;
		imageRange.baseArrayLayer = 0;
		imageRange.layerCount = 1;
		auto info = VkImageViewCreateInfo{};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		info.format = SwapchainInstance->getImageFormat();
		info.components.r = VK_COMPONENT_SWIZZLE_R;
		info.components.g = VK_COMPONENT_SWIZZLE_G;
		info.components.b = VK_COMPONENT_SWIZZLE_B;
		info.components.a = VK_COMPONENT_SWIZZLE_A;
		info.subresourceRange = imageRange;
		for (uint32_t i = 0; i < imageCount; i++)
		{
			auto* fd = &frames[i];
			info.image = fd->Backbuffer;
			vkCreateImageView(device.getDevice(), &info, nullptr, &fd->BackbufferView);
		}

		auto imageAttachment = VkImageView{};
		auto framebufferInfo = VkFramebufferCreateInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = igRenderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &imageAttachment;
		framebufferInfo.width = SwapchainInstance->getWindowExtent().width;
		framebufferInfo.height = SwapchainInstance->getWindowExtent().height;
		framebufferInfo.layers = 1;
		for (uint32_t i = 0; i < 3; i++)
		{
			auto* fd = &frames[i];
			imageAttachment = fd->BackbufferView;
			vkCreateFramebuffer(DeviceInstance.getDevice(), &framebufferInfo, nullptr, &fd->Framebuffer);
		}

		// Create Command Buffers
		for (uint32_t i = 0; i < imageCount; i++)
		{
			auto* fd = &frames[i];
			auto* fsd = &semaphores[i];
			{
				VkCommandPoolCreateInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
				info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
				info.queueFamilyIndex = device.getQueueFamilyIndices().graphicsFamily;
				vkCreateCommandPool(device.getDevice(), &info, nullptr, &fd->CommandPool);
			}
			{
				VkCommandBufferAllocateInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
				info.commandPool = fd->CommandPool;
				info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				info.commandBufferCount = 1;
				vkAllocateCommandBuffers(device.getDevice(), &info, &fd->CommandBuffer);
			}
			{
				VkFenceCreateInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
				info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
				vkCreateFence(device.getDevice(), &info, nullptr, &fd->Fence);
			}
			{
				VkSemaphoreCreateInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
				vkCreateSemaphore(device.getDevice(), &info, nullptr, &fsd->ImageAcquiredSemaphore);
				vkCreateSemaphore(device.getDevice(), &info, nullptr, &fsd->RenderCompleteSemaphore);
			}
		}
	}

}
