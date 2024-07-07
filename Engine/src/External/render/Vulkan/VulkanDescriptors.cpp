#include "VulkanDescriptors.h"
#include "Context.h"

#include <algorithm>

#include "Engine/Core/Assert.h"

#include "Swapchain.h"

namespace
{

	constexpr VkDescriptorType descType2VkType(const RT::DescriptorType descriptorType)
	{
		switch (descriptorType)
		{
			case RT::DescriptorType::Uniform: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			case RT::DescriptorType::Storage: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			case RT::DescriptorType::Sampler: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			case RT::DescriptorType::Image:   return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		}
		return VkDescriptorType{};
	}

}

namespace RT::Vulkan
{

	VulkanDescriptorLayout::VulkanDescriptorLayout(const DescriptorLayoutSpec& spec)
	{
		auto bindings = std::vector<VkDescriptorSetLayoutBinding>(spec.size());

		for (uint32_t i = 0; i < spec.size(); i++)
		{
			const auto [count, setType] = spec[i];
			bindings[i].binding = i;
			bindings[i].descriptorType = descType2VkType(setType);
			bindings[i].descriptorCount = count;
			bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_ALL_GRAPHICS;
		}

		auto descriptorSetLayoutInfo = VkDescriptorSetLayoutCreateInfo{};
		descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutInfo.bindingCount = bindings.size();
		descriptorSetLayoutInfo.pBindings = bindings.data();

		RT_CORE_ASSERT(
			vkCreateDescriptorSetLayout(
				DeviceInstance.getDevice(),
				&descriptorSetLayoutInfo,
				nullptr,
				&descriptorLayout) == VK_SUCCESS,
			"failed to create descriptor set layout!");
	}

	VulkanDescriptorLayout::~VulkanDescriptorLayout()
	{
		vkDestroyDescriptorSetLayout(DeviceInstance.getDevice(), descriptorLayout, nullptr);
	}

	VulkanDescriptorPool::VulkanDescriptorPool(const DescriptorPoolSpec& spec)
	{
		auto vkSpec = VulkanDescriptorPoolSpec(spec.size());
		for (uint32_t i = 0; i < spec.size(); i++)
		{
			const auto [count, setType] = spec[i];
			vkSpec[i].type = descType2VkType(setType);
			vkSpec[i].descriptorCount = count;
		}

		auto descriptorPoolInfo = VkDescriptorPoolCreateInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		descriptorPoolInfo.maxSets = Swapchain::MAX_FRAMES_IN_FLIGHT;
		descriptorPoolInfo.poolSizeCount = vkSpec.size();
		descriptorPoolInfo.pPoolSizes = vkSpec.data();

		RT_CORE_ASSERT(
			vkCreateDescriptorPool(
				DeviceInstance.getDevice(),
				&descriptorPoolInfo,
				nullptr,
				&descriptorPool) == VK_SUCCESS,
			"failed to create descriptor pool!");
	}

	VulkanDescriptorPool::~VulkanDescriptorPool()
	{
		vkDestroyDescriptorPool(DeviceInstance.getDevice(), descriptorPool, nullptr);
	}

	VulkanDescriptorSet::VulkanDescriptorSet(const DescriptorLayout& rtLayout, const DescriptorPool& rtPool)
		: layout{static_cast<const VulkanDescriptorLayout&>(rtLayout)}
		, pool{static_cast<const VulkanDescriptorPool&>(rtPool)}
	{
		auto pSetLayout = std::array {layout.getLayout()};

		auto allocInfo = VkDescriptorSetAllocateInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = pool.getPool();
		allocInfo.descriptorSetCount = pSetLayout.size();
		allocInfo.pSetLayouts = pSetLayout.data();

		RT_CORE_ASSERT(
			vkAllocateDescriptorSets(
				DeviceInstance.getDevice(),
				&allocInfo,
				&set) == VK_SUCCESS,
			"failed to create descriptor set!");
	}

	VulkanDescriptorSet::~VulkanDescriptorSet()
	{
		vkFreeDescriptorSets(
			DeviceInstance.getDevice(),
			pool.getPool(),
			1,
			&set);
	}

	void VulkanDescriptorSet::write(const uint32_t binding, const Uniform& uniform) const
	{
		const auto& vkUniform = static_cast<const VulkanUniform&>(uniform);

		auto write = VkWriteDescriptorSet{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = set;
		write.dstBinding = binding;
		switch (vkUniform.uniformType)
		{
			case UniformType::Uniform:
				write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				write.pBufferInfo = &vkUniform.bufferInfo;
				break;
			case UniformType::Storage:
				write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				write.pBufferInfo = &vkUniform.bufferInfo;
				break;
			case UniformType::Sampler:
				write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				write.pImageInfo = &vkUniform.imgInfo;
				break;
			case UniformType::Image:
				write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				write.pImageInfo = &vkUniform.imgInfo;
				break;
		}
		write.descriptorCount = 1;

		vkUpdateDescriptorSets(DeviceInstance.getDevice(), 1, &write, 0, nullptr);
	}

	void VulkanDescriptorSet::bind(const uint32_t binding) const
	{
		if (Context::frameBindings.size() <= binding)
		{
			Context::frameBindings.resize(binding + 1);
		}
		Context::frameBindings[binding] = set;
	}

}
