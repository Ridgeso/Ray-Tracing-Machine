#pragma once
#include <vulkan/vulkan.h>

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
        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        VkPipelineColorBlendStateCreateInfo colorBlendInfo = {};
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo = {};
        std::vector<VkDynamicState> dynamicStatesEnables = {};
        VkPipelineDynamicStateCreateInfo dynamicStateInfo = {};
        VkPipelineLayout pipelineLayout = nullptr;
        VkRenderPass renderPass = nullptr;
        uint32_t subpass = 0;
    };

	class Pipeline
	{
    public:
        Pipeline() = default;
        ~Pipeline() = default;

        Pipeline(const Pipeline&) = delete;
        Pipeline(Pipeline&&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;
        Pipeline&& operator=(Pipeline&&) = delete;

        void init(const std::string& shaderName, const PipelineConfigInfo& configInfo);
        void shutdown();
        
        VkPipeline getGraphicsPipeline() const { return graphicsPipeline; }
        void bind(const VkCommandBuffer commandBuffer) const;

        static void defaultPipelineConfigInfo(PipelineConfigInfo& configInfo);

    private:
        void createGraphicsPipeline(const std::string& shaderName, const PipelineConfigInfo& configInfo);

    private:
        VkPipeline graphicsPipeline = {};
        VulkanShader shader = {};
	};

}
