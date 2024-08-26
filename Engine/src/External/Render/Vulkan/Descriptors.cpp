#include "Descriptors.h"
#include "Context.h"

#include "utils/Debug.h"

#include "Swapchain.h"

namespace
{

	constexpr VkDescriptorType uniformType2VkDescriptorType(const RT::UniformType uniformType)
	{
		switch (uniformType)
		{
			case RT::UniformType::Uniform: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			case RT::UniformType::Storage: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			case RT::UniformType::Sampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			case RT::UniformType::Image:   return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		}
		return VkDescriptorType{};
	}

}

namespace RT::Vulkan
{

	Descriptors::Descriptors(const UniformLayouts& uniformLayouts)
	{
		RT_LOG_INFO("Creating Descriptor: {{ nrOfLayouts = {} }}", uniformLayouts.size());
		createLayout(uniformLayouts);
		createPool(uniformLayouts);
		allocateSets(uniformLayouts);
		RT_LOG_INFO("Descriptor created");
	}

	Descriptors::~Descriptors()
	{
		for (const auto& sets : layoutSets)
		{
			vkFreeDescriptorSets(
				DeviceInstance.getDevice(),
				descriptorPool,
				sets.size(),
				(VkDescriptorSet*)sets.data());
		}

		vkDestroyDescriptorPool(DeviceInstance.getDevice(), descriptorPool, nullptr);

		for (auto descriptorLayout : descriptorLayouts)
		{
			vkDestroyDescriptorSetLayout(DeviceInstance.getDevice(), descriptorLayout, nullptr);
		}
	}

	void Descriptors::write(
		const uint32_t layout,
		const uint32_t set,
		const uint32_t binding,
		const VulkanUniform& uniform) const
	{
		auto writeSets = MultiVkWriteDescriptorSet{};
		uint32_t setNr = 0u;
		for (auto& write : writeSets)
		{
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.dstSet = layoutSets[layout][set][setNr];
			write.dstBinding = binding;
			write.descriptorType = uniformType2VkDescriptorType(uniform.uniformType);
			write.descriptorCount = 1;
			write.pBufferInfo = uniform.getWriteBufferInfo(setNr);
			setNr++;
		}

		vkUpdateDescriptorSets(DeviceInstance.getDevice(), writeSets.size(), writeSets.data(), 0, nullptr);
	}
	
	void Descriptors::write(
		const uint32_t layout,
		const uint32_t set,
		const uint32_t binding,
		const VulkanTexture& sampler,
		const RT::UniformType samplerType) const
	{
		const auto info = sampler.getWriteImageInfo();
		auto writeSets = MultiVkWriteDescriptorSet{};
		uint32_t setNr = 0u;
		for (auto& write : writeSets)
		{
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.dstSet = layoutSets[layout][set][setNr];
			write.dstBinding = binding;
			write.descriptorType = uniformType2VkDescriptorType(samplerType);
			write.descriptorCount = 1;
			write.pImageInfo = &info;
			setNr++;
		}

		vkUpdateDescriptorSets(DeviceInstance.getDevice(), writeSets.size(), writeSets.data(), 0, nullptr);
	}

	VkDescriptorSet Descriptors::currFrameSet(const uint32_t layout, const uint32_t set) const
	{
		return layoutSets[layout][set][SwapchainInstance->getCurrentFrame()];
	}

	void Descriptors::createLayout(const UniformLayouts& uniformLayouts)
	{
		for (const auto& uniformLayout : uniformLayouts)
		{
			auto bindings = std::vector<VkDescriptorSetLayoutBinding>(uniformLayout.layout.size());

			for (uint32_t i = 0; i < uniformLayout.layout.size(); i++)
			{
				bindings[i].binding = i;
				bindings[i].descriptorType = uniformType2VkDescriptorType(uniformLayout.layout[i]);
				bindings[i].descriptorCount = uniformLayout.nrOfSets;
				bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_ALL_GRAPHICS;
			}

			auto descriptorSetLayoutInfo = VkDescriptorSetLayoutCreateInfo{};
			descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptorSetLayoutInfo.bindingCount = bindings.size();
			descriptorSetLayoutInfo.pBindings = bindings.data();

			auto& descriptorLayout = descriptorLayouts.emplace_back();

			CHECK_VK(
				vkCreateDescriptorSetLayout(
					DeviceInstance.getDevice(),
					&descriptorSetLayoutInfo,
					nullptr,
					&descriptorLayout),
				"failed to create descriptor set layout!");
		}
	}

	void Descriptors::createPool(const UniformLayouts& uniformLayouts)
	{
		auto descriptors = std::unordered_map<UniformType, uint32_t>{};
		uint32_t setsCount = 0u;
		for (const auto& uniformLayout : uniformLayouts)
		{
			setsCount += uniformLayout.nrOfSets;
			for (const auto layout : uniformLayout.layout)
			{
				descriptors[layout] += uniformLayout.nrOfSets * Constants::MAX_FRAMES_IN_FLIGHT;
			}
		}

		auto pools = std::vector<VkDescriptorPoolSize>(descriptors.size());
		int32_t poolIdx = 0;
		for (auto [type, count] : descriptors)
		{
			pools[poolIdx].type = uniformType2VkDescriptorType(type);
			pools[poolIdx].descriptorCount = count;
			poolIdx++;
		}

		auto descriptorPoolInfo = VkDescriptorPoolCreateInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		descriptorPoolInfo.maxSets = setsCount * Constants::MAX_FRAMES_IN_FLIGHT;
		descriptorPoolInfo.poolSizeCount = pools.size();
		descriptorPoolInfo.pPoolSizes = pools.data();

		CHECK_VK(
			vkCreateDescriptorPool(
				DeviceInstance.getDevice(),
				&descriptorPoolInfo,
				nullptr,
				&descriptorPool),
			"failed to create descriptor pool!");
	}

	void Descriptors::allocateSets(const UniformLayouts& uniformLayouts)
	{
		layoutSets.resize(uniformLayouts.size());
		for (int32_t layoutIdx = 0; layoutIdx < uniformLayouts.size(); layoutIdx++)
		{
			auto& sets = layoutSets[layoutIdx];

			auto setsLayouts = std::vector<MultiVkDescriptorSetLayout>(uniformLayouts[layoutIdx].nrOfSets);
			for (auto& setsLayout : setsLayouts)
			{
				setsLayout.fill(descriptorLayouts[layoutIdx]);
			}

			auto allocInfo = VkDescriptorSetAllocateInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = setsLayouts.size() * Constants::MAX_FRAMES_IN_FLIGHT;
			allocInfo.pSetLayouts = (const VkDescriptorSetLayout*)setsLayouts.data();

			sets.resize(setsLayouts.size());

			CHECK_VK(
				vkAllocateDescriptorSets(
					DeviceInstance.getDevice(),
					&allocInfo,
					(VkDescriptorSet*)sets.data()),
				"failed to create descriptor set!");
		}
	}

}
