#include "VulkanPipeline.h"
#include "Context.h"

#include "utils/Debug.h"

#include "VulkanBuffer.h"
#include "VulkanRenderPass.h"

namespace RT::Vulkan
{

    VulkanPipeline::ConfigInfo VulkanPipeline::ConfigInfo::defaultConfig()
    {
        auto configInfo = VulkanPipeline::ConfigInfo{};

        configInfo.inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        configInfo.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        configInfo.inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

        configInfo.viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        configInfo.viewportInfo.viewportCount = 1;
        configInfo.viewportInfo.pViewports = nullptr;
        configInfo.viewportInfo.scissorCount = 1;
        configInfo.viewportInfo.pScissors = nullptr;

        configInfo.rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        configInfo.rasterizationInfo.depthClampEnable = VK_FALSE;
        configInfo.rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
        configInfo.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
        configInfo.rasterizationInfo.lineWidth = 1.0f;
        configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
        configInfo.rasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
        configInfo.rasterizationInfo.depthBiasEnable = VK_FALSE;
        configInfo.rasterizationInfo.depthBiasConstantFactor = 0.0f;
        configInfo.rasterizationInfo.depthBiasClamp = 0.0f;
        configInfo.rasterizationInfo.depthBiasSlopeFactor = 0.0f;

        configInfo.multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        configInfo.multisampleInfo.sampleShadingEnable = VK_FALSE;
        configInfo.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        configInfo.multisampleInfo.minSampleShading = 1.0f;
        configInfo.multisampleInfo.pSampleMask = nullptr;
        configInfo.multisampleInfo.alphaToCoverageEnable = VK_FALSE;
        configInfo.multisampleInfo.alphaToOneEnable = VK_FALSE;

        {
            auto& colorBlendAttachment = configInfo.colorBlendAttachment.emplace_back();
            colorBlendAttachment.colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachment.blendEnable = VK_FALSE;
            colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        }
        {
            auto& colorBlendAttachment = configInfo.colorBlendAttachment.emplace_back();
            colorBlendAttachment.colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachment.blendEnable = VK_FALSE;
            colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        }

        configInfo.colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        configInfo.colorBlendInfo.logicOpEnable = VK_FALSE;
        configInfo.colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
        configInfo.colorBlendInfo.attachmentCount = configInfo.colorBlendAttachment.size();
        configInfo.colorBlendInfo.pAttachments = configInfo.colorBlendAttachment.data();
        configInfo.colorBlendInfo.blendConstants[0] = 0.0f;
        configInfo.colorBlendInfo.blendConstants[1] = 0.0f;
        configInfo.colorBlendInfo.blendConstants[2] = 0.0f;
        configInfo.colorBlendInfo.blendConstants[3] = 0.0f;

        configInfo.depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        configInfo.depthStencilInfo.depthTestEnable = VK_TRUE;
        configInfo.depthStencilInfo.depthWriteEnable = VK_TRUE;
        configInfo.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
        configInfo.depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
        configInfo.depthStencilInfo.minDepthBounds = 0.0f;
        configInfo.depthStencilInfo.maxDepthBounds = 1.0f;
        configInfo.depthStencilInfo.stencilTestEnable = VK_FALSE;
        configInfo.depthStencilInfo.front = {};
        configInfo.depthStencilInfo.back = {};

        configInfo.dynamicStatesEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        configInfo.dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        configInfo.dynamicStateInfo.dynamicStateCount = configInfo.dynamicStatesEnables.size();
        configInfo.dynamicStateInfo.pDynamicStates = configInfo.dynamicStatesEnables.data();
        configInfo.dynamicStateInfo.flags = 0;

        return configInfo;
    }

    VulkanPipeline::VulkanPipeline(PipelineSpec& spec)
        : layouts{std::move(spec.uniformLayouts)}, descriptors{layouts}
    {
        createPipelineLayout();

        auto pipelineConfigInfo = ConfigInfo::defaultConfig();
        pipelineConfigInfo.pipelineLayout = pipelineLayout;
        
        auto shader = Shader(spec.shaderPath);
        if (shader.isCompute())
        {
            RT_LOG_INFO("Creating Pipeline: {{ mode = compute }}");
            createComputePipeline(pipelineConfigInfo, shader);
        }
        else
        {
            RT_LOG_INFO("Creating Pipeline: {{ mode = graphics }}");
            createGraphicsPipeline(pipelineConfigInfo, shader, spec.attachmentFormats);
        }
        RT_LOG_INFO("Pipeline created");
    }

    VulkanPipeline::~VulkanPipeline()
    {
        auto device = DeviceInstance.getDevice();
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyPipeline(device, pipeline, nullptr);
    }

    void VulkanPipeline::updateSet(const uint32_t layout, const uint32_t set, const uint32_t binding, const Uniform& uniform) const
    {
        descriptors.write(layout, set, binding, static_cast<const VulkanUniform&>(uniform));
    }

    void VulkanPipeline::updateSet(const uint32_t layout, const uint32_t set, const uint32_t binding, const Texture& sampler) const
    {
        descriptors.write(layout, set, binding, static_cast<const VulkanTexture&>(sampler), layouts[layout].layout[binding].type);
    }

    void VulkanPipeline::updateSet(const uint32_t layout, const uint32_t set, const uint32_t binding, const TextureArray& samplers) const
    {
        descriptors.write(layout, set, binding, samplers, layouts[layout].layout[binding].type);
    }

    void VulkanPipeline::bindSet(const uint32_t layout, const uint32_t set) const
    {
        if (bindingSets.size() <= layout)
        {
            bindingSets.resize(layout + 1);
        }
        bindingSets[layout] = descriptors.currFrameSet(layout, set);
    }

    void VulkanPipeline::bind() const
    {
        bindDescriptors<VK_PIPELINE_BIND_POINT_GRAPHICS>();
        vkCmdBindPipeline(Context::frameCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    }

    void VulkanPipeline::dispatch(const glm::uvec2 groups) const
    {
        bindDescriptors<VK_PIPELINE_BIND_POINT_COMPUTE>();
        vkCmdBindPipeline(Context::frameCmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        const auto dispatchGroup = groups / group + groups % group;
        vkCmdDispatch(Context::frameCmd, dispatchGroup.x, dispatchGroup.y, 1);
    }

    template <VkPipelineBindPoint PipelinePoint>
    void VulkanPipeline::bindDescriptors() const
    {
        vkCmdBindDescriptorSets(
            Context::frameCmd,
            PipelinePoint,
            pipelineLayout,
            0,
            bindingSets.size(),
            bindingSets.data(),
            0,
            nullptr);
        bindingSets.clear();
    }
    
    void VulkanPipeline::createPipelineLayout()
    {
        auto pipelineLayoutInfo = VkPipelineLayoutCreateInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = descriptors.layouts().size();
        pipelineLayoutInfo.pSetLayouts = descriptors.layouts().data();
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        CHECK_VK(
            vkCreatePipelineLayout(DeviceInstance.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout),
            "Could not create pipeline Layout");
    }

    void VulkanPipeline::createComputePipeline(ConfigInfo& configInfo, const Shader& shader)
    {
        auto shaderStages = shader.getStages();

        auto pipelineInfo = VkComputePipelineCreateInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = shaderStages[0];

        pipelineInfo.layout = configInfo.pipelineLayout;

        pipelineInfo.basePipelineIndex = -1;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        CHECK_VK(
            vkCreateComputePipelines(
                DeviceInstance.getDevice(),
                VK_NULL_HANDLE,
                1,
                &pipelineInfo,
                nullptr,
                &pipeline),
            "failed to create graphics pipeline");
    }

    void VulkanPipeline::createGraphicsPipeline(
        ConfigInfo& configInfo,
        const Shader& shader,
        const AttachmentFormats& formats)
    {
        auto renderPass = VulkanRenderPass(formats);
        configInfo.renderPass = renderPass.getRenderPass();

        auto shaderStages = shader.getStages();

        auto attriDesc = Vertex::getAttributeDescriptions();
        auto bindDesc = Vertex::getBindingDescriptions();
        auto vertexInputInfo = VkPipelineVertexInputStateCreateInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexAttributeDescriptionCount = attriDesc.size();
        vertexInputInfo.vertexBindingDescriptionCount = bindDesc.size();
        vertexInputInfo.pVertexAttributeDescriptions = attriDesc.data();
        vertexInputInfo.pVertexBindingDescriptions = bindDesc.data();

        auto pipelineInfo = VkGraphicsPipelineCreateInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = shaderStages.size();
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &configInfo.inputAssemblyInfo;
        pipelineInfo.pViewportState = &configInfo.viewportInfo;
        pipelineInfo.pRasterizationState = &configInfo.rasterizationInfo;
        pipelineInfo.pMultisampleState = &configInfo.multisampleInfo;

        pipelineInfo.pColorBlendState = &configInfo.colorBlendInfo;
        pipelineInfo.pDepthStencilState = &configInfo.depthStencilInfo;
        pipelineInfo.pDynamicState = &configInfo.dynamicStateInfo;

        pipelineInfo.layout = configInfo.pipelineLayout;
        pipelineInfo.renderPass = configInfo.renderPass;
        pipelineInfo.subpass = configInfo.subpass;

        pipelineInfo.basePipelineIndex = -1;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        CHECK_VK(
            vkCreateGraphicsPipelines(
                DeviceInstance.getDevice(),
                VK_NULL_HANDLE,
                1,
                &pipelineInfo,
                nullptr,
                &pipeline),
            "failed to create graphics pipeline");
    }

}
