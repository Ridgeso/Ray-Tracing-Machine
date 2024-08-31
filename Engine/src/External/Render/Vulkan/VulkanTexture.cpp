#include "VulkanTexture.h"
#include "Context.h"

#include "Engine/Utils/LogDefinitions.h"

#include "utils/Debug.h"
#include "Device.h"
#include "Swapchain.h"
#include "utils/Utils.h"

#include <backends/imgui_impl_vulkan.h>

#include "stb_image.h"

namespace RT::Vulkan
{

	VulkanTexture::VulkanTexture(const std::filesystem::path& path, const Filter filter, const Mode mode)
		: format{Format::RGBA8}
		, mode{mode}
		, filter{filter}
	{
		stbi_set_flip_vertically_on_load(1);
		RT_LOG_INFO("Loading Texture: {{ path = {} }}", path);

		int32_t bytesPerPixel = 0;
		auto* data = stbi_load(path.string().c_str(), (int32_t*)&size.x, (int32_t*)&size.y, &bytesPerPixel, STBI_rgb_alpha);
		if (data == nullptr)
		{
			RT_LOG_WARN("Couldn't load texture");
			return;
		}
		
		imSize = calcImSize();

		initVulkanImage(true);
		setBuffer(data);

		RT_LOG_INFO("Texture loaded: {{ size = {}, imageFormat = {} }}", size, RT::Utils::imageFormat2Str(format));
		stbi_image_free(data);
	}
	
	VulkanTexture::VulkanTexture(const glm::uvec2 size, const Format imageFormat)
		: size{size}
		, format{imageFormat}
		, filter{Filter::Nearest}
		, mode{Mode::ClampToBorder}
		, imSize{calcImSize()}
	{
		RT_LOG_INFO("Creating Texture: {{ size = {}, imageFormat = {} }}", size, RT::Utils::imageFormat2Str(format));
		initVulkanImage(false);
		RT_LOG_INFO("Texture Created");
	}

	VulkanTexture::~VulkanTexture()
	{
		DeviceInstance.waitForIdle();
		auto device = DeviceInstance.getDevice();

		ImGui_ImplVulkan_RemoveTexture(descriptorSet);
		vkDestroySampler(device, sampler, nullptr);
		vkDestroyImageView(device, imageView, nullptr);
		vkDestroyImage(device, image, nullptr);
		vkFreeMemory(device, memory, nullptr);
		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	void VulkanTexture::setBuffer(const void* data)
	{
		uploadToBuffer(data);
		copyToImage();
	}

	void VulkanTexture::transition(const Access imageAccess, const Layout imageLayout) const
	{
		DeviceInstance.execSingleCmdPass([&](const auto cmdBuffer) -> void
		{
			auto dstAccessMask = imageAccess2VulkanAccess(imageAccess);
			auto newLayout = imageLayout2VulkanLayout(imageLayout);
			
			vulkanBarrier(cmdBuffer, dstAccessMask, newLayout);

			currAccessMask = dstAccessMask;
			currLayout = newLayout;
		});
	}

	void VulkanTexture::barrier(
		const Access imageAccess,
		const Layout imageLayout) const
	{
		auto dstAccessMask = imageAccess2VulkanAccess(imageAccess);
		auto newLayout = imageLayout2VulkanLayout(imageLayout);

		vulkanBarrier(Context::frameCmd, dstAccessMask, newLayout);

		currAccessMask = dstAccessMask;
		currLayout = newLayout;
	}

	void VulkanTexture::vulkanBarrier(
		const VkCommandBuffer cmdBuff,
		const VkAccessFlags dstAccessMask,
		const VkImageLayout newLayout) const
	{
		auto barrier = VkImageMemoryBarrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.pNext = nullptr;
		barrier.srcAccessMask = currAccessMask;
		barrier.dstAccessMask = dstAccessMask;
		barrier.oldLayout = currLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange = subresourceRange;

		vkCmdPipelineBarrier(
			cmdBuff,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier);
	}

	const VkFormat VulkanTexture::imageFormat2VulkanFormat(const Format imageFormat)
	{
		switch (imageFormat)
		{
			case Format::R8:	  return VK_FORMAT_R8_UNORM;
			case Format::RGB8:	  return VK_FORMAT_R8G8B8_UNORM;
			case Format::RGBA8:	  return VK_FORMAT_R8G8B8A8_UNORM;
			case Format::RGBA32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
		}
		return VK_FORMAT_UNDEFINED;
	}

	const uint32_t VulkanTexture::imageFormat2Size(const Format imageFormat)
	{
		switch (imageFormat)
		{
			case Format::R8:	  return 1;
			case Format::RGB8:	  return 3;
			case Format::RGBA8:	  return 4;
			case Format::RGBA32F: return 4 * 4;
			case Format::Depth:	  return 0;
		}
		return 0;
	}

	const VkImageLayout VulkanTexture::imageLayout2VulkanLayout(const Layout imageLayout)
	{
		switch (imageLayout)
		{
			case Layout::Undefined:  return VK_IMAGE_LAYOUT_UNDEFINED;
			case Layout::General:    return VK_IMAGE_LAYOUT_GENERAL;
			case Layout::ShaderRead: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		return VK_IMAGE_LAYOUT_UNDEFINED;
	}

	const VkAccessFlags VulkanTexture::imageAccess2VulkanAccess(const Access imageAccess)
	{
		switch (imageAccess)
		{
			case Access::None:  return VK_ACCESS_NONE;
			case Access::Write: return VK_ACCESS_SHADER_WRITE_BIT;
			case Access::Read:  return VK_ACCESS_SHADER_READ_BIT;
		}
		return VK_ACCESS_NONE;
	}

	const VkSamplerAddressMode VulkanTexture::imageMode2VulkanMode(const Mode imageMode)
	{
		switch (imageMode)
		{
			case Mode::Repeat:		  return VK_SAMPLER_ADDRESS_MODE_REPEAT;
			case Mode::Mirrored:	  return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			case Mode::ClampToEdge:   return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			case Mode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		}
		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	}

	void VulkanTexture::initVulkanImage(const bool isFromMemory)
	{
		createImage(isFromMemory);

		allocateMemory();
		allocateStaginBuffer();
		createImageView();
		createSampler(isFromMemory);

		if (Format::Depth != format)
		{
			descriptorSet = ImGui_ImplVulkan_AddTexture(
				sampler,
				imageView,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}

		imageInfo.sampler = sampler;
		imageInfo.imageView = imageView;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}

	void VulkanTexture::createImage(const bool isFromMemory)
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

		if (Format::Depth == format)
		{
			imageCreateInfo.format = Swapchain::findDepthFormat();
			imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
		else
		{
			imageCreateInfo.format = imageFormat2VulkanFormat(format);
			imageCreateInfo.usage = isFromMemory ?
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT :
				VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

				//VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
				//VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
				//VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}

		CHECK_VK(
			vkCreateImage(DeviceInstance.getDevice(), &imageCreateInfo, nullptr, &image),
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

		CHECK_VK(
			vkAllocateMemory(device, &allocInfo, nullptr, &memory),
			"failed to allocate image memory!");
		CHECK_VK(
			vkBindImageMemory(device, image, memory, 0),
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
		CHECK_VK(
			vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer),
			"failed to create staging buffer!");

		auto memReq = VkMemoryRequirements{};
		vkGetBufferMemoryRequirements(device, stagingBuffer, &memReq);

		auto allocInfo = VkMemoryAllocateInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memReq.size;
		allocInfo.memoryTypeIndex = DeviceInstance.findMemoryType(
			memReq.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		CHECK_VK(
			vkAllocateMemory(device, &allocInfo, nullptr, &stagingBufferMemory),
			"failed to allocate image memory!");
		CHECK_VK(
			vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0),
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
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;

		if (Format::Depth == format)
		{
			viewInfo.format = Swapchain::findDepthFormat();
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		else
		{
			viewInfo.format = imageFormat2VulkanFormat(format);
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}

		CHECK_VK(
			vkCreateImageView(DeviceInstance.getDevice(), &viewInfo, nullptr, &imageView),
			"failed to create texture image view!");
	}

	void VulkanTexture::createSampler(const bool isFromMemory)
	{
		auto info = VkSamplerCreateInfo{};
		info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		info.magFilter = Filter::Nearest == filter ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
		info.minFilter = Filter::Nearest == filter ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
		info.mipmapMode = Filter::Nearest == filter ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
		info.addressModeU = imageMode2VulkanMode(mode);
		info.addressModeV = imageMode2VulkanMode(mode);
		info.addressModeW = imageMode2VulkanMode(mode);
		info.minLod = -1000;
		info.maxLod = 1000;
		info.maxAnisotropy = 1.0f;
		info.compareOp = VK_COMPARE_OP_NEVER;
		CHECK_VK(
			vkCreateSampler(DeviceInstance.getDevice(), &info, nullptr, &sampler),
			"failed to create texture image sampler!");
	}

	void VulkanTexture::uploadToBuffer(const void* data)
	{
		auto device = DeviceInstance.getDevice();

		void* dstData = nullptr;
		vkMapMemory(device, stagingBufferMemory, 0, imSize, 0, &dstData);

		std::memcpy(dstData, data, imSize);

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
			vulkanBarrier(cmdBuffer, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

			auto region = VkBufferImageCopy{};
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.layerCount = 1;
			region.imageExtent.width = size.x;
			region.imageExtent.height = size.y;
			region.imageExtent.depth = 1;
			vkCmdCopyBufferToImage(cmdBuffer, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

			vulkanBarrier(cmdBuffer, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		});
	}

}
