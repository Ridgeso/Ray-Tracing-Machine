#pragma once
#include "Engine/Render/Texture.h"

#include <vulkan/vulkan.h>

namespace RT::Vulkan
{

	class VulkanTexture : public Texture
	{
	public:
		VulkanTexture(const glm::uvec2 size, const ImageFormat imageFormat);
		~VulkanTexture() final;

		void setBuff(const void* data) final;

		const ImTextureID getTexId() const final { return descriptorSet; }
		const glm::uvec2 getSize() const final { return size; }

		void transition(const ImageAccess imageAccess, const ImageLayout imageLayout) const final;
		void barrier(const ImageAccess imageAccess, const ImageLayout imageLayout) const final;

		VkImageView getImageView() const { return imageView; }
		VkSampler getSampler() const { return sampler; }

		void vulkanBarrier(
			const VkCommandBuffer cmdBuff,
			const VkAccessFlags dstAccessMask,
			const VkImageLayout newLayout) const;

		const VkDescriptorImageInfo getWriteImageInfo() const;

		static constexpr VkFormat imageFormat2VulkanFormat(const ImageFormat imageFormat);
		static constexpr uint32_t format2Size(const ImageFormat imageFormat);
		static constexpr VkImageLayout imageLayout2VulkanLayout(const ImageLayout imageLayout);
		static constexpr VkAccessFlags imageAccess2VulkanAccess(const ImageAccess imageAccess);

	private:
		void createImage();
		void allocateMemory();
		void allocateStaginBuffer();
		void createImageView();
		void createSampler();

		void uploadToBuffer();
		void copyToImage();

	private:
		glm::uvec2 size = {};
		ImageFormat format = {};
		size_t imSize = 0u;

		VkImage image = {};
		VkImageView imageView = {};
		VkDeviceMemory memory = {};
		VkSampler sampler = {};

		// TODO: will be needed when writing to image memory
		VkBuffer stagingBuffer = {};
		VkDeviceMemory stagingBufferMemory = {};

		VkDescriptorSet descriptorSet = {};
		mutable VkAccessFlags currAccessMask = VK_ACCESS_NONE;
		mutable VkImageLayout currLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		static inline constexpr VkImageSubresourceRange subresourceRange = VkImageSubresourceRange{
			VK_IMAGE_ASPECT_COLOR_BIT,
			0,
			1,
			0,
			1};
	};

}
