#pragma once
#include <vector>

#include "Engine/Render/Pipeline.h"

#include "Swapchain.h"

#include <vulkan/vulkan.h>

namespace RT::Vulkan
{

	class Descriptors
	{
	public:
		Descriptors(const UniformLayouts& uniformLayouts);
		~Descriptors();

		void write(const uint32_t layout, const uint32_t set, const uint32_t binding, const Uniform& uniform) const;

		const std::vector<VkDescriptorSetLayout>& layouts() const { return descriptorLayouts; }
		const std::vector<std::vector<VkDescriptorSet>>& sets() const { return layoutSets;}

	private:
		void createLayout(const UniformLayouts& uniformLayouts);
		void createPool(const UniformLayouts& uniformLayouts);
		void allocateSets(const UniformLayouts& uniformLayouts);

	private:
		std::vector<VkDescriptorSetLayout> descriptorLayouts = {};
		VkDescriptorPool descriptorPool = {};
		std::vector<std::vector<VkDescriptorSet>> layoutSets = {};
	};

}
