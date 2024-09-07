#include "Swapchain.h"
#include "utils/Debug.h"

namespace RT::Vulkan
{

    Local<Swapchain> Swapchain::swapchainInstance = nullptr;

    Swapchain::Swapchain(const VkExtent2D windowExtent, const Share<Swapchain>& oldSwapchain)
        : windowExtent{windowExtent}, oldSwapchain{oldSwapchain}
    {
    }

    void Swapchain::init()
    {
        RT_LOG_DEBUG("Initializing Swapchain");
        createSwapChain();
        createImageViews();
        createRenderPass();
        createFramebuffers();
        createSyncObjects();

        oldSwapchain = nullptr;
    }

    void Swapchain::shutdown()
    {
        const auto device = DeviceInstance.getDevice();

        for (auto imageView : swapChainImageViews)
        {
            vkDestroyImageView(device, imageView, nullptr);
        }
        swapChainImageViews.clear();

        if (swapChain != nullptr)
        {
            vkDestroySwapchainKHR(device, swapChain, nullptr);
            swapChain = nullptr;
        }

        for (auto framebuffer : swapChainFramebuffers)
        {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }

        vkDestroyRenderPass(device, renderPass, nullptr);

        for (size_t i = 0; i < Constants::MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }
    }

    VkResult Swapchain::acquireNextImage(uint32_t& imageIndex)
    {
        const auto device = DeviceInstance.getDevice();
        vkWaitForFences(
            device,
            1,
            &inFlightFences[currentFrame],
            VK_TRUE,
            std::numeric_limits<uint64_t>::max());

        return vkAcquireNextImageKHR(
            device,
            swapChain,
            std::numeric_limits<uint64_t>::max(),
            imageAvailableSemaphores[currentFrame],
            VK_NULL_HANDLE,
            &imageIndex);
    }

    VkResult Swapchain::submitCommandBuffers(const VkCommandBuffer& frameBuffer, const VkCommandBuffer& guiBuffer, uint32_t& imageIndex)
    {
        const auto& deviceInstance = DeviceInstance;

        vkResetFences(deviceInstance.getDevice(), 1, &inFlightFences[currentFrame]);
        
        auto submitInfo = VkSubmitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        constexpr auto waitStages = std::array<VkPipelineStageFlags, 1>{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        auto waitStagesSemaphores = std::array{ imageAvailableSemaphores[currentFrame] };
        submitInfo.waitSemaphoreCount = waitStagesSemaphores.size();
        submitInfo.pWaitSemaphores = waitStagesSemaphores.data();
        submitInfo.pWaitDstStageMask = waitStages.data();

        const auto buffers = std::array{ frameBuffer, guiBuffer };
        submitInfo.commandBufferCount = buffers.size();
        submitInfo.pCommandBuffers = buffers.data();

        auto signalSemaphores = std::array{ renderFinishedSemaphores[currentFrame] };
        submitInfo.signalSemaphoreCount = signalSemaphores.size();
        submitInfo.pSignalSemaphores = signalSemaphores.data();

        CHECK_VK(
            vkQueueSubmit(deviceInstance.getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]),
            "failed to submit draw command buffer!");

        auto presentInfo = VkPresentInfoKHR{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = signalSemaphores.size();
        presentInfo.pWaitSemaphores = signalSemaphores.data();

        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapChain;
        presentInfo.pImageIndices = &imageIndex;

        incrementFrameCounter();
        return vkQueuePresentKHR(deviceInstance.getPresentQueue(), &presentInfo);
    }

    bool Swapchain::compareFormats(const Swapchain& other) const
    {
        return other.swapChainDepthFormat == swapChainDepthFormat &&
            other.swapChainImageFormat == swapChainImageFormat;
    }

    VkFormat Swapchain::findDepthFormat()
    {
        return DeviceInstance.findSupportedFormat(
            { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    void Swapchain::createSwapChain()
    {
        auto& deviceInstance = DeviceInstance;

        // TODO: swapChainSupportDetails needs to be recalculated during window sizing. Probably cashing it is bad idea!
        // auto swapChainSupport = deviceInstance.getSwapChainSupportDetails();
        auto swapChainSupport = deviceInstance.querySwapChainSupport(deviceInstance.getPhysicalDevice());

        auto surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        auto presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        auto extent = chooseSwapExtent(swapChainSupport.capabilities);

        imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 &&
            imageCount > swapChainSupport.capabilities.maxImageCount)
        {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        auto createInfo = VkSwapchainCreateInfoKHR{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = deviceInstance.getSurface();

        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        auto indices = deviceInstance.getQueueFamilyIndices();
        auto queueFamilyIndices = std::vector<uint32_t>{ indices.graphicsFamily, indices.presentFamily };

        if (indices.graphicsFamily != indices.presentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        createInfo.oldSwapchain = oldSwapchain == nullptr ? VK_NULL_HANDLE : oldSwapchain->swapChain;

        CHECK_VK(
            vkCreateSwapchainKHR(deviceInstance.getDevice(), &createInfo, nullptr, &swapChain),
            "failed to create swap chain!");

        vkGetSwapchainImagesKHR(deviceInstance.getDevice(), swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(deviceInstance.getDevice(), swapChain, &imageCount, swapChainImages.data());

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;

        RT_LOG_DEBUG("Swapchain created: {{ buffering = {}, extent = [{}, {}], V-Sync = {} }}",
            imageCount,
            extent.width, extent.height,
            VK_PRESENT_MODE_FIFO_KHR == presentMode);
    }

    void Swapchain::createImageViews()
    {
        swapChainImageViews.resize(swapChainImages.size());
        for (size_t i = 0; i < swapChainImages.size(); i++)
        {
            auto viewInfo = VkImageViewCreateInfo{};
            viewInfo.image = swapChainImages[i];
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = swapChainImageFormat;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            CHECK_VK(
                vkCreateImageView(DeviceInstance.getDevice(), &viewInfo, nullptr, &swapChainImageViews[i]),
                "failed to create texture image view!");
        }
    }

    void Swapchain::createRenderPass()
    {
        auto depthAttachment = VkAttachmentDescription{};
        depthAttachment.format = findDepthFormat();
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        auto depthAttachmentRef = VkAttachmentReference{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        auto colorAttachment = VkAttachmentDescription{};
        colorAttachment.format = swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        auto colorAttachmentRef = VkAttachmentReference{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        auto subpass = VkSubpassDescription{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = VK_NULL_HANDLE;

        auto dependency = VkSubpassDependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        auto attachments = std::array<VkAttachmentDescription, 1>{ colorAttachment };

        auto renderPassInfo = VkRenderPassCreateInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        CHECK_VK(
            vkCreateRenderPass(DeviceInstance.getDevice(), &renderPassInfo, nullptr, &renderPass),
            "failed to create render pass!");
    }

    void Swapchain::createFramebuffers()
    {
        swapChainFramebuffers.resize(swapChainImages.size());
        for (size_t i = 0; i < swapChainImages.size(); i++)
        {
            auto attachments = std::array<VkImageView, 1>{ swapChainImageViews[i] };

            auto framebufferInfo = VkFramebufferCreateInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;

            CHECK_VK(
                vkCreateFramebuffer(
                    DeviceInstance.getDevice(),
                    &framebufferInfo,
                    nullptr,
                    &swapChainFramebuffers[i]),
                "failed to create framebuffer!");
        }
    }

    void Swapchain::createSyncObjects()
    {
        imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

        auto semaphoreInfo = VkSemaphoreCreateInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        auto fenceInfo = VkFenceCreateInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < Constants::MAX_FRAMES_IN_FLIGHT; i++)
        {
            CHECK_VK(
                vkCreateSemaphore(DeviceInstance.getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]),
                "failed to create render available synchronization objects for a frame!");
            CHECK_VK(
                vkCreateSemaphore(DeviceInstance.getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]),
                "failed to create render finish synchronization objects for a frame!");
            CHECK_VK(
                vkCreateFence(DeviceInstance.getDevice(), &fenceInfo, nullptr, &inFlightFences[i]),
                "failed to create render fence synchronization objects for a frame!");
        }
    }

    VkExtent2D Swapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }
         
        auto actualExtent = windowExtent;

        actualExtent.width = std::max(
            capabilities.minImageExtent.width,
            std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(
            capabilities.minImageExtent.height,
            std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }

    bool Swapchain::compareSwapFormats(const Swapchain& swapChain) const
    {
        return swapChain.swapChainDepthFormat == swapChainDepthFormat &&
            swapChain.swapChainImageFormat == swapChainImageFormat;
    }

    void Swapchain::incrementFrameCounter()
    {
        currentFrame = (currentFrame + 1) % Constants::MAX_FRAMES_IN_FLIGHT;
    }

    VkSurfaceFormatKHR Swapchain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
    {
        for (const auto& availableFormat : availableFormats)
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    VkPresentModeKHR Swapchain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
    {
        for (const auto& availablePresentMode : availablePresentModes)
        {
           if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
           {
               RT_LOG_WARN("Present mode: Mailbox");
               return availablePresentMode;
           }
        }

        for (const auto& availablePresentMode : availablePresentModes)
        {
           if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
           {
               RT_LOG_WARN("Present mode: Immediate");
               return availablePresentMode;
           }
        }

        RT_LOG_INFO("Present mode: V-Sync");
        return VK_PRESENT_MODE_FIFO_KHR;
    }

}
