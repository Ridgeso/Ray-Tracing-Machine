#pragma once
#include <vector>

#include "Swapchain.h"

#include <vulkan/vulkan.h>

namespace RT::Vulkan
{

	struct DescriptorSpec
	{
		std::vector<VkDescriptorType> bindTypes = {};
		std::vector<VkDescriptorPoolSize> poolSizes = {};
		uint32_t nrOfBindings = 1;
		uint32_t maxSets = Swapchain::MAX_FRAMES_IN_FLIGHT;
	};

	class Descriptor
	{
	public:

	public:
		Descriptor(const DescriptorSpec& spec);
		~Descriptor();

		void writeUniform(const uint32_t set, const uint32_t binding, VkDescriptorBufferInfo bufferInfo);
		void writeStorage(const uint32_t set, const uint32_t binding, VkDescriptorBufferInfo bufferInfo);
		void writeImage(const uint32_t set, const uint32_t binding, VkDescriptorImageInfo imgInfo);

		const VkDescriptorSet* getSet(const uint32_t set) const { return &descriptor[set]; }
		const VkDescriptorSetLayout* getLayout() const { return &descriptorSetLayout; }

	private:
		std::vector<VkDescriptorSet> descriptor = {};
		VkDescriptorSetLayout descriptorSetLayout = {};
		VkDescriptorPool descriptorPool = {};
	};

}
