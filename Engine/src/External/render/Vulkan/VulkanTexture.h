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

		VkImageView getImageView() const { return imageView; }
		VkSampler getSampler() const { return sampler; }

		void barrier(
			const VkCommandBuffer commandBuffer,
			const VkImageSubresourceRange subresourceRange,
			const VkAccessFlags srcAccessMask,
			const VkAccessFlags dstAccessMask,
			const VkImageLayout oldLayout,
			const VkImageLayout newLayout) const;
		
		static constexpr VkFormat imageFormat2VulkanFormat(const ImageFormat imageFormat);
		static constexpr uint32_t format2Size(const ImageFormat imageFormat);

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
	};

}
