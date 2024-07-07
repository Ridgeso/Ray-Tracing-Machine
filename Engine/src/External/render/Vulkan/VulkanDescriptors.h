#pragma once
#include <vector>
#include <tuple>

#include "Engine/Render/Descriptors.h"

#include "Swapchain.h"
#include "VulkanBuffer.h"

#include <vulkan/vulkan.h>

namespace RT::Vulkan
{

	using VulkanDescriptorLayoutSpec =std::vector<
		std::tuple<
			uint32_t,
			VkDescriptorType>>;

	class VulkanDescriptorLayout : public DescriptorLayout
	{
	public:
		VulkanDescriptorLayout(const DescriptorLayoutSpec& spec);
		~VulkanDescriptorLayout() final;

		const VkDescriptorSetLayout getLayout() const { return descriptorLayout; }

	private:
		VkDescriptorSetLayout descriptorLayout = {};
	};
	
	using VulkanDescriptorPoolSpec = std::vector<VkDescriptorPoolSize>;

	class VulkanDescriptorPool : public DescriptorPool
	{
	public:
		VulkanDescriptorPool(const DescriptorPoolSpec& spec);
		~VulkanDescriptorPool() final;

		const VkDescriptorPool getPool() const { return descriptorPool; }

	private:
		VkDescriptorPool descriptorPool = {};
	};

	class VulkanDescriptorSet : public DescriptorSet
	{
	public:
		VulkanDescriptorSet(const DescriptorLayout& rtLayout, const DescriptorPool& rtPool);
		~VulkanDescriptorSet() final;

		void write(const uint32_t binding, const Uniform& uniform) const final;
		void bind(const uint32_t binding) const final;

	private:
		VkDescriptorSet set = {};
		
		const VulkanDescriptorLayout& layout;
		const VulkanDescriptorPool& pool;
	};

}
