#pragma once
#include <vulkan/vulkan.h>

#include <vector>

#include "Engine/Render/Pipeline.h"

#include "Device.h"
#include "VulkanShader.h"

namespace RT::Vulkan
{

    struct PipelineConfigInfo
    {
        PipelineConfigInfo() = default;
        PipelineConfigInfo(const PipelineConfigInfo&) = delete;
        PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;

        VkPipelineViewportStateCreateInfo viewportInfo = {};
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {};
        VkPipelineRasterizationStateCreateInfo rasterizationInfo = {};
        VkPipelineMultisampleStateCreateInfo multisampleInfo = {};
        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachment = {};
        VkPipelineColorBlendStateCreateInfo colorBlendInfo = {};
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo = {};
        std::vector<VkDynamicState> dynamicStatesEnables = {};
        VkPipelineDynamicStateCreateInfo dynamicStateInfo = {};
        VkPipelineLayout pipelineLayout = nullptr;
        VkRenderPass renderPass = nullptr;
        uint32_t subpass = 0;
    };

	class VulkanPipeline : public Pipeline
	{
    public:
        VulkanPipeline() = default;
        ~VulkanPipeline() = default;

        VulkanPipeline(const VulkanPipeline&) = delete;
        VulkanPipeline(VulkanPipeline&&) = delete;
        VulkanPipeline& operator=(const VulkanPipeline&) = delete;
        VulkanPipeline&& operator=(VulkanPipeline&&) = delete;

        void init(const Shader& shader, const RenderPass& renderPass, const PipelineLayouts& pipelineLayouts) override;
        void shutdown() final;
        
        VkPipeline getGraphicsPipeline() const { return graphicsPipeline; }
        void bind(const VkCommandBuffer commandBuffer) const;
        const VkPipelineLayout getLayout() const { return pipelineLayout; }

        static void defaultPipelineConfigInfo(PipelineConfigInfo& configInfo);

    private:
        void createGraphicsPipeline(const VulkanShader& shader, const PipelineConfigInfo& configInfo);

    private:
        VkPipeline graphicsPipeline = {};
        VkPipelineLayout pipelineLayout = {};
	};

}
