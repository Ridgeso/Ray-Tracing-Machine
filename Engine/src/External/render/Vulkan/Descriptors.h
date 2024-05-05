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

		void writeUniform(const uint32_t binding, VkDescriptorBufferInfo bufferInfo);
		void writeStorage(const uint32_t binding, VkDescriptorBufferInfo bufferInfo);
		void writeImage(const uint32_t binding, VkDescriptorImageInfo imgInfo);

		const VkDescriptorSet* getSet() const { return &descriptor; }
		const VkDescriptorSetLayout* getLayout() const { return &descriptorSetLayout; }

	private:
		VkDescriptorSet descriptor = {};
		VkDescriptorSetLayout descriptorSetLayout = {};
		VkDescriptorPool descriptorPool = {};
	};

	void bindDescriptor(const uint32_t binding, const Descriptor& descriptor);

	std::vector<VkDescriptorSet> getBindedDescriptors();
	std::vector<VkDescriptorSetLayout> getRegistredLayouts();
}
