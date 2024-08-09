#pragma once
#include <vulkan/vulkan.h>

#include <vector>

#include "Engine/Render/Pipeline.h"

#include "Device.h"
#include "Shader.h"

#include "Descriptors.h"

namespace RT::Vulkan
{

	class VulkanPipeline : public Pipeline
	{
    private:
        struct ConfigInfo
        {
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

            static ConfigInfo defaultConfig();
        };

    public:
        VulkanPipeline(PipelineSpec& spec);
        ~VulkanPipeline() final;

        VulkanPipeline(const VulkanPipeline&) = delete;
        VulkanPipeline(VulkanPipeline&&) = delete;
        VulkanPipeline& operator=(const VulkanPipeline&) = delete;
        VulkanPipeline&& operator=(VulkanPipeline&&) = delete;

        void updateSet(const uint32_t layout, const uint32_t set, const uint32_t binding, const Uniform& uniform) const final;
        void updateSet(const uint32_t layout, const uint32_t set, const uint32_t binding, const Texture& sampler) const final;
        void bindSet(const uint32_t layout, const uint32_t set) const final;

        void bind() const final;
        void dispatch(const glm::uvec2 groups) const final;
        
    private:
        template <VkPipelineBindPoint PipelinePoint>
        void bindDescriptors() const;

        void createPipelineLayout();

        void createComputePipeline(ConfigInfo& configInfo, const Shader& shader);
        void createGraphicsPipeline(
            ConfigInfo& configInfo,
            const Shader& shader,
            const AttachmentFormats& formats);

    private:
        VkPipeline pipeline = {};
        VkPipelineLayout pipelineLayout = {};

        UniformLayouts layouts = {};
        Descriptors descriptors;
        mutable std::vector<VkDescriptorSet> bindingSets = {};
    };

}
