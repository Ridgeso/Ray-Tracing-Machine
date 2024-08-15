#pragma once
#include <array>
#include <vector>

#include "Engine/Render/Pipeline.h"

#include "VulkanBuffer.h"
#include "VulkanTexture.h"
#include "utils/Constants.h"

#include <vulkan/vulkan.h>

namespace RT::Vulkan
{

	class Descriptors
	{
	private:
		template <typename Set> using MultiVkSet = std::array<Set, Constants::MAX_FRAMES_IN_FLIGHT>;
		using MultiVkDescriptorSet = MultiVkSet<VkDescriptorSet>;
		using MultiVkDescriptorSetLayout = MultiVkSet<VkDescriptorSetLayout>;
		using MultiVkWriteDescriptorSet = MultiVkSet<VkWriteDescriptorSet>;

	public:
		Descriptors(const UniformLayouts& uniformLayouts);
		~Descriptors();

		void write(const uint32_t layout, const uint32_t set, const uint32_t binding, const VulkanUniform& uniform) const;
		void write(
			const uint32_t layout,
			const uint32_t set,
			const uint32_t binding,
			const VulkanTexture& sampler,
			const RT::UniformType samplerType) const;
		VkDescriptorSet currFrameSet(const uint32_t layout, const uint32_t set) const;

		const std::vector<VkDescriptorSetLayout>& layouts() const { return descriptorLayouts; }

	private:
		void createLayout(const UniformLayouts& uniformLayouts);
		void createPool(const UniformLayouts& uniformLayouts);
		void allocateSets(const UniformLayouts& uniformLayouts);

	private:
		std::vector<VkDescriptorSetLayout> descriptorLayouts = {};
		VkDescriptorPool descriptorPool = {};
		std::vector<std::vector<MultiVkDescriptorSet>> layoutSets = {};
	};

}
