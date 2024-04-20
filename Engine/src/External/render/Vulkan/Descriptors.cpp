#include "Descriptors.h"

#include <algorithm>

#include "Engine/Core/Assert.h"

#include "Swapchain.h"

namespace RT::Vulkan
{

	Descriptor::Descriptor(const DescriptorSpec& spec)
	{
		auto bindings = std::vector<VkDescriptorSetLayoutBinding>(spec.bindTypes.size());

		for (uint32_t i = 0; i < spec.bindTypes.size(); i++)
		{
			bindings[i].binding = i;
			bindings[i].descriptorType = spec.bindTypes[i];
			bindings[i].descriptorCount = spec.nrOfBindings;
			bindings[i].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
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
				&descriptorSetLayout) == VK_SUCCESS,
			"failed to create descriptor set layout!");

		auto descriptorPoolInfo = VkDescriptorPoolCreateInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		descriptorPoolInfo.maxSets = spec.maxSets;
		descriptorPoolInfo.poolSizeCount = spec.poolSizes.size();
		descriptorPoolInfo.pPoolSizes = spec.poolSizes.data();

		RT_CORE_ASSERT(
			vkCreateDescriptorPool(
				DeviceInstance.getDevice(),
				&descriptorPoolInfo,
				nullptr,
				&descriptorPool) == VK_SUCCESS,
			"failed to create descriptor pool!");

		auto descriptorSetLayouts = std::vector<VkDescriptorSetLayout>(spec.maxSets);
		std::fill(descriptorSetLayouts.begin(), descriptorSetLayouts.end(), descriptorSetLayout);

		auto allocInfo = VkDescriptorSetAllocateInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.pSetLayouts = descriptorSetLayouts.data();
		allocInfo.descriptorSetCount = spec.maxSets;

		descriptor.resize(spec.maxSets);
		RT_ASSERT(vkAllocateDescriptorSets(DeviceInstance.getDevice(), &allocInfo, descriptor.data()) == VK_SUCCESS);
	}

	Descriptor::~Descriptor()
	{
		const auto device = DeviceInstance.getDevice();

		vkFreeDescriptorSets(
			device,
			descriptorPool,
			descriptor.size(),
			descriptor.data());

		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	}

	void Descriptor::writeUniform(const uint32_t set, const uint32_t binding, VkDescriptorBufferInfo bufferInfo)
	{
		auto write = VkWriteDescriptorSet{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = descriptor[set];
		write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		write.dstBinding = binding;
		write.pBufferInfo = &bufferInfo;
		write.descriptorCount = 1;

		vkUpdateDescriptorSets(DeviceInstance.getDevice(), 1, &write, 0, nullptr);
	}

	void Descriptor::writeStorage(const uint32_t set, const uint32_t binding, VkDescriptorBufferInfo bufferInfo)
	{
		auto write = VkWriteDescriptorSet{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = descriptor[set];
		write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write.dstBinding = binding;
		write.pBufferInfo = &bufferInfo;
		write.descriptorCount = 1;

		vkUpdateDescriptorSets(DeviceInstance.getDevice(), 1, &write, 0, nullptr);
	}

	void Descriptor::writeImage(const uint32_t set, const uint32_t binding, VkDescriptorImageInfo imgInfo)
	{
		auto write = VkWriteDescriptorSet{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = descriptor[set];
		write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		write.dstBinding = binding;
		write.pImageInfo = &imgInfo;
		write.descriptorCount = 1;

		vkUpdateDescriptorSets(DeviceInstance.getDevice(), 1, &write, 0, nullptr);
	}

}
