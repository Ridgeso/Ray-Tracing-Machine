#include "VulkanTexture.h"
#include "Engine/Core/Assert.h"

#include "Device.h"
#include "Swapchain.h"
#include "utils/Utils.h"

#include <backends/imgui_impl_vulkan.h>

namespace RT::Vulkan
{

	VulkanTexture::VulkanTexture(const glm::uvec2 size, const ImageFormat imageFormat)
		: size{size}
		, format{imageFormat}
		, imSize{size.x * size.y * format2Size(format)}
	{
		createImage();

		allocateMemory();
		allocateStaginBuffer();
		createImageView();
		createSampler();

		if (ImageFormat::Depth != imageFormat)
		{
			descriptorSet = ImGui_ImplVulkan_AddTexture(
				sampler,
				imageView,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			setBuff(nullptr);
		}

	}

	VulkanTexture::~VulkanTexture()
	{
		auto device = DeviceInstance.getDevice();
		vkDeviceWaitIdle(device);

		ImGui_ImplVulkan_RemoveTexture(descriptorSet);
		vkDestroySampler(device, sampler, nullptr);
		vkDestroyImageView(device, imageView, nullptr);
		vkDestroyImage(device, image, nullptr);
		vkFreeMemory(device, memory, nullptr);
		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	void VulkanTexture::setBuff(const void* data)
	{
		uploadToBuffer();
		copyToImage();
	}

	constexpr VkFormat VulkanTexture::imageFormat2VulkanFormat(const ImageFormat imageFormat)
	{
		switch (imageFormat)
		{
			case ImageFormat::R8:		return VK_FORMAT_R8_UNORM;
			case ImageFormat::RGB8:		return VK_FORMAT_R8G8B8_UNORM;
			case ImageFormat::RGBA8:	return VK_FORMAT_R8G8B8A8_UNORM;
			case ImageFormat::RGBA32F:	return VK_FORMAT_R32G32B32A32_SFLOAT;
		}
		return VK_FORMAT_UNDEFINED;
	}

	constexpr uint32_t VulkanTexture::format2Size(const ImageFormat imageFormat)
	{
		switch (imageFormat)
		{
			case ImageFormat::R8:		return 1;
			case ImageFormat::RGB8:		return 3;
			case ImageFormat::RGBA8:	return 4;
			case ImageFormat::RGBA32F:	return 4 * 4;
			case ImageFormat::Depth:	return 0;
		}
		return 0;
	}

	void VulkanTexture::createImage()
	{
		auto imageCreateInfo = VkImageCreateInfo{};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.extent.width = size.x;
		imageCreateInfo.extent.height = size.y;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.flags = 0;

		if (ImageFormat::Depth == format)
		{
			imageCreateInfo.format = Swapchain::findDepthFormat();
			imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
		else
		{
			imageCreateInfo.format = imageFormat2VulkanFormat(format);
			imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
				VK_IMAGE_USAGE_SAMPLED_BIT |
				VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}

		RT_CORE_ASSERT(
			vkCreateImage(DeviceInstance.getDevice(), &imageCreateInfo, nullptr, &image) == VK_SUCCESS,
			"failed to create texture image!");
	}

	void VulkanTexture::allocateMemory()
	{
		auto device = DeviceInstance.getDevice();

		auto memReq = VkMemoryRequirements{};
		vkGetImageMemoryRequirements(device, image, &memReq);

		auto allocInfo = VkMemoryAllocateInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memReq.size;
		allocInfo.memoryTypeIndex = DeviceInstance.findMemoryType(
			memReq.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		RT_CORE_ASSERT(
			vkAllocateMemory(device, &allocInfo, nullptr, &memory) == VK_SUCCESS,
			"failed to allocate image memory!");
		RT_CORE_ASSERT(
			vkBindImageMemory(device, image, memory, 0) == VK_SUCCESS,
			"failed to bind image memory!");

		imSize = memReq.size;
	}

	void VulkanTexture::allocateStaginBuffer()
	{
		auto device = DeviceInstance.getDevice();

		auto bufferInfo = VkBufferCreateInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = imSize;
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		RT_CORE_ASSERT(
			vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer) == VK_SUCCESS,
			"failed to create staging buffer!");

		auto memReq = VkMemoryRequirements{};
		vkGetBufferMemoryRequirements(device, stagingBuffer, &memReq);

		auto allocInfo = VkMemoryAllocateInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memReq.size;
		allocInfo.memoryTypeIndex = DeviceInstance.findMemoryType(
			memReq.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		RT_CORE_ASSERT(
			vkAllocateMemory(device, &allocInfo, nullptr, &stagingBufferMemory) == VK_SUCCESS,
			"failed to allocate image memory!");
		RT_CORE_ASSERT(
			vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0) == VK_SUCCESS,
			"failed to bind image memory!");
	}

	void VulkanTexture::createImageView()
	{
		auto viewInfo = VkImageViewCreateInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		
		if (ImageFormat::Depth == format)
		{
			viewInfo.format = Swapchain::findDepthFormat();
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		else
		{
			viewInfo.format = imageFormat2VulkanFormat(format);
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}

		RT_CORE_ASSERT(
			vkCreateImageView(DeviceInstance.getDevice(), &viewInfo, nullptr, &imageView) == VK_SUCCESS,
			"failed to create texture image view!");
	}

	void VulkanTexture::createSampler()
	{
		auto info = VkSamplerCreateInfo{};
		info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		info.magFilter = VK_FILTER_LINEAR;
		info.minFilter = VK_FILTER_LINEAR;
		info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		info.minLod = -1000;
		info.maxLod = 1000;
		info.maxAnisotropy = 1.0f;
		RT_CORE_ASSERT(
			vkCreateSampler(DeviceInstance.getDevice(), &info, nullptr, &sampler) == VK_SUCCESS,
			"failed to create texture image sampler!");
	}

	void VulkanTexture::uploadToBuffer()
	{
		auto device = DeviceInstance.getDevice();

		void* dstData = nullptr;
		vkMapMemory(device, stagingBufferMemory, 0, imSize, 0, &dstData);

		// just for testing RGBA32F format
		for (uint32_t i = 0; i < imSize / sizeof(float); i += 4)
		{
			((float*)dstData)[i + 0] = 1.0f;
			((float*)dstData)[i + 1] = 0.2f;
			((float*)dstData)[i + 2] = 0.5f;
			((float*)dstData)[i + 3] = 1.0f;
		}
		//std::memset(dstData, 255, imSize);

		auto range = VkMappedMemoryRange{};
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory = stagingBufferMemory;
		range.size = imSize;
		vkFlushMappedMemoryRanges(device, 1, &range);

		vkUnmapMemory(device, stagingBufferMemory);
	}

	void VulkanTexture::copyToImage()
	{
		DeviceInstance.execSingleCmdPass([this](const auto cmdBuffer) -> void
		{
			auto copyBarrier = VkImageMemoryBarrier{};
			copyBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			copyBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			copyBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			copyBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			copyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			copyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			copyBarrier.image = image;
			copyBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyBarrier.subresourceRange.levelCount = 1;
			copyBarrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &copyBarrier);

			auto region = VkBufferImageCopy{};
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.layerCount = 1;
			region.imageExtent.width = size.x;
			region.imageExtent.height = size.y;
			region.imageExtent.depth = 1;
			vkCmdCopyBufferToImage(cmdBuffer, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

			auto useBarrier = VkImageMemoryBarrier{};
			useBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			useBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			useBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			useBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			useBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			useBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			useBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			useBarrier.image = image;
			useBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			useBarrier.subresourceRange.levelCount = 1;
			useBarrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &useBarrier);
		});
	}

}
