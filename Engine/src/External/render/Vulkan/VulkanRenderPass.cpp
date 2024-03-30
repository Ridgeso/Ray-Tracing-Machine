#include <tuple>

#include "VulkanRenderPass.h"
#include "Engine/Core/Assert.h"
#include "Device.h"
#include "Swapchain.h"

namespace RT::Vulkan
{

    VulkanRenderPass::VulkanRenderPass(const RenderPassSpec& spec)
        : size{spec.size}
    {
        createAttachments(spec.attachmentsFormats);
        createRenderPass(spec.attachmentsFormats);
        createFrameBuffers();
    }

    VulkanRenderPass::~VulkanRenderPass()
    {
        const auto device = DeviceInstance.getDevice();

        attachments.clear();

        for (auto& frameBuffer : frameBuffers)
        {
            vkDestroyFramebuffer(device, frameBuffer, nullptr);
        }
        frameBuffers.clear();

        vkDestroyRenderPass(device, renderPass, nullptr);
    }
    
    void VulkanRenderPass::bind() const
    {
    }

    void VulkanRenderPass::unbind() const
    {
    }

    void VulkanRenderPass::createAttachments(const std::vector<ImageFormat>& attachmentTypes)
    {
        attachments.reserve(attachmentTypes.size());
        for (int32_t i = 0; i < attachmentTypes.size(); i++)
        {
            attachments.emplace_back(size, attachmentTypes[i]);
        }
    }

    void VulkanRenderPass::createRenderPass(const std::vector<ImageFormat>& attachmentTypes)
    {
        auto colorAtt = std::make_tuple(
            std::vector<VkAttachmentDescription>(),
            std::vector<VkAttachmentReference>());
        auto depthAtt = std::make_tuple(
            std::vector<VkAttachmentDescription>(),
            std::vector<VkAttachmentReference>());
        auto allAttDesc = std::vector<VkAttachmentDescription>(attachmentTypes.size());

        for (int32_t i = 0; i < attachmentTypes.size(); i++)
        {
            const bool isDepth = ImageFormat::Depth == attachmentTypes[i];

            allAttDesc[i].format = isDepth ?
                Swapchain::findDepthFormat() :
                VulkanTexture::imageFormat2VulkanFormat(attachmentTypes[i]);
            allAttDesc[i].samples = VK_SAMPLE_COUNT_1_BIT;
            allAttDesc[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            allAttDesc[i].storeOp = isDepth ?
                VK_ATTACHMENT_STORE_OP_DONT_CARE :
                VK_ATTACHMENT_STORE_OP_STORE;
            allAttDesc[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            allAttDesc[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            allAttDesc[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            allAttDesc[i].finalLayout = isDepth ?
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            auto attRef = VkAttachmentReference{};
            attRef.attachment = i;
            attRef.layout = isDepth ?
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            if (isDepth)
            {
                std::get<0>(depthAtt).push_back(allAttDesc[i]);
                std::get<1>(depthAtt).push_back(attRef);
            }
            else
            {
                std::get<0>(colorAtt).push_back(allAttDesc[i]);
                std::get<1>(colorAtt).push_back(attRef);
            }
        }

        auto subpass = VkSubpassDescription{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = std::get<1>(colorAtt).size();
        subpass.pColorAttachments = std::get<1>(colorAtt).data();
        subpass.pDepthStencilAttachment = std::get<1>(depthAtt).data();

        auto dependency = VkSubpassDependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        auto renderPassInfo = VkRenderPassCreateInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(allAttDesc.size());
        renderPassInfo.pAttachments = allAttDesc.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        RT_CORE_ASSERT(
            vkCreateRenderPass(DeviceInstance.getDevice(), &renderPassInfo, nullptr, &renderPass) == VK_SUCCESS,
            "failed to create render pass!");
    }

    void VulkanRenderPass::createFrameBuffers()
    {
        frameBuffers.resize(SwapchainInstance->getImageCount());
        for (int32_t i = 0; i < frameBuffers.size(); i++)
        {
            auto infoAttachments = std::vector<VkImageView>(attachments.size());
            for (int32_t i = 0; i < attachments.size(); i++)
            {
                infoAttachments[i] = attachments[i].getImageView();
            }

            auto framebufferInfo = VkFramebufferCreateInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(infoAttachments.size());
            framebufferInfo.pAttachments = infoAttachments.data();
            framebufferInfo.width = size.x;
            framebufferInfo.height = size.y;
            framebufferInfo.layers = 1;

            RT_CORE_ASSERT(
                vkCreateFramebuffer(
                    DeviceInstance.getDevice(),
                    &framebufferInfo,
                    nullptr,
                    &frameBuffers[i]) == VK_SUCCESS,
                "failed to create framebuffer!");
        }
    }

}
