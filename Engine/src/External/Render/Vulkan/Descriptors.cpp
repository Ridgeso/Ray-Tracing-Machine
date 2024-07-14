#include "Descriptors.h"
#include "Context.h"

#include "Engine/Core/Assert.h"

#include "Swapchain.h"
#include "VulkanBuffer.h"

namespace
{

	constexpr VkDescriptorType descriptorType2VkType(const RT::UniformType uniformType)
	{
		switch (uniformType)
		{
			case RT::UniformType::Uniform: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			case RT::UniformType::Storage: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			case RT::UniformType::Sampler: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			case RT::UniformType::Image:   return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		}
		return VkDescriptorType{};
	}

}

namespace RT::Vulkan
{

	Descriptors::Descriptors(const UniformLayouts& uniformLayouts)
	{
		createLayout(uniformLayouts);
		createPool(uniformLayouts);
		allocateSets(uniformLayouts);
	}

	Descriptors::~Descriptors()
	{
		for (const auto& sets : layoutSets)
		{
			vkFreeDescriptorSets(
				DeviceInstance.getDevice(),
				descriptorPool,
				sets.size(),
				sets.data());
		}

		vkDestroyDescriptorPool(DeviceInstance.getDevice(), descriptorPool, nullptr);

		for (auto descriptorLayout : descriptorLayouts)
		{
			vkDestroyDescriptorSetLayout(DeviceInstance.getDevice(), descriptorLayout, nullptr);
		}
	}

	void Descriptors::createLayout(const UniformLayouts& uniformLayouts)
	{
		for (const auto& uniformLayout : uniformLayouts)
		{
			auto bindings = std::vector<VkDescriptorSetLayoutBinding>(uniformLayout.layout.size());

			for (uint32_t i = 0; i < uniformLayout.layout.size(); i++)
			{
				bindings[i].binding = i;
				bindings[i].descriptorType = descriptorType2VkType(uniformLayout.layout[i]);
				bindings[i].descriptorCount = uniformLayout.nrOfSets;
				bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_ALL_GRAPHICS;
			}

			auto descriptorSetLayoutInfo = VkDescriptorSetLayoutCreateInfo{};
			descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptorSetLayoutInfo.bindingCount = bindings.size();
			descriptorSetLayoutInfo.pBindings = bindings.data();

			auto& descriptorLayout = descriptorLayouts.emplace_back();

			RT_CORE_ASSERT(
				vkCreateDescriptorSetLayout(
					DeviceInstance.getDevice(),
					&descriptorSetLayoutInfo,
					nullptr,
					&descriptorLayout) == VK_SUCCESS,
				"failed to create descriptor set layout!");
		}
	}

	void Descriptors::createPool(const UniformLayouts& uniformLayouts)
	{
		auto descriptors = std::unordered_map<UniformType, uint32_t>{};
		for (const auto& uniformLayout : uniformLayouts)
		{
			for (int32_t i = 0; i < uniformLayout.layout.size(); i++)
			{
				descriptors[uniformLayout.layout[i]] += uniformLayout.nrOfSets;
			}
		}

		auto pools = std::vector<VkDescriptorPoolSize>(descriptors.size());
		int32_t poolIdx = 0;
		for (auto [type, count] : descriptors)
		{
			pools[poolIdx].type = descriptorType2VkType(type);
			pools[poolIdx].descriptorCount = count;
			poolIdx++;
		}

		auto descriptorPoolInfo = VkDescriptorPoolCreateInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		descriptorPoolInfo.maxSets = Swapchain::MAX_FRAMES_IN_FLIGHT;
		descriptorPoolInfo.poolSizeCount = pools.size();
		descriptorPoolInfo.pPoolSizes = pools.data();

		RT_CORE_ASSERT(
			vkCreateDescriptorPool(
				DeviceInstance.getDevice(),
				&descriptorPoolInfo,
				nullptr,
				&descriptorPool) == VK_SUCCESS,
			"failed to create descriptor pool!");
	}

	void Descriptors::allocateSets(const UniformLayouts& uniformLayouts)
	{
		layoutSets.resize(uniformLayouts.size());
		for (int32_t layoutIdx = 0; layoutIdx < uniformLayouts.size(); layoutIdx++)
		{
			auto& sets = layoutSets[layoutIdx];

			auto setsLayouts = std::vector<VkDescriptorSetLayout>(uniformLayouts[layoutIdx].nrOfSets);
			for (int32_t i = 0; i < uniformLayouts[layoutIdx].nrOfSets; i++)
			{
				setsLayouts[i] = descriptorLayouts[layoutIdx];
			}

			auto allocInfo = VkDescriptorSetAllocateInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = setsLayouts.size();
			allocInfo.pSetLayouts = setsLayouts.data();

			sets.resize(setsLayouts.size());

			RT_CORE_ASSERT(
				vkAllocateDescriptorSets(
					DeviceInstance.getDevice(),
					&allocInfo,
					sets.data()) == VK_SUCCESS,
				"failed to create descriptor set!");
		}
	}

	void Descriptors::write(
		const uint32_t layout,
		const uint32_t set,
		const uint32_t binding,
		const Uniform& uniform) const
	{
		const auto& vkUniform = static_cast<const VulkanUniform&>(uniform);

		auto write = VkWriteDescriptorSet{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = layoutSets[layout][set];
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

}
