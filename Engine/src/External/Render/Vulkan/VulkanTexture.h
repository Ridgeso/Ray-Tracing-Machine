#pragma once
#include "Engine/Render/Texture.h"

#include <vulkan/vulkan.h>

namespace RT::Vulkan
{

	class VulkanTexture final : public Texture
	{
	public:
		VulkanTexture(const std::filesystem::path& path);
		VulkanTexture(const glm::uvec2 size, const ImageFormat imageFormat);
		~VulkanTexture() final;

		void setBuffer(const void* data) final;

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

		static const VkFormat imageFormat2VulkanFormat(const ImageFormat imageFormat);
		static const uint32_t imageFormat2Size(const ImageFormat imageFormat);
		static const VkImageLayout imageLayout2VulkanLayout(const ImageLayout imageLayout);
		static const VkAccessFlags imageAccess2VulkanAccess(const ImageAccess imageAccess);

	private:
		void initVulkanImage(const bool isFromMemory);
		void createImage(const bool isFromMemory);
		void allocateMemory();
		void allocateStaginBuffer();
		void createImageView();
		void createSampler();

		void uploadToBuffer(const void* data);
		void copyToImage();
		const size_t calcImSize() const { return size.x * size.y * imageFormat2Size(format); }

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
