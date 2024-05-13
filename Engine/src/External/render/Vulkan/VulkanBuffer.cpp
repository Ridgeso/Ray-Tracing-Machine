#include "VulkanBuffer.h"

#include "Engine/Core/Log.h"
#include "Engine/Core/Assert.h"

#include "Swapchain.h"
#include "VulkanTexture.h"

namespace RT::Vulkan
{

	auto uniformsToFlush = std::vector<VulkanUniform*>();

	VulkanVertexBuffer::VulkanVertexBuffer(const uint32_t size)
	{
		auto vertices = std::vector<Vertex>(size);
		createVertexBuffers(vertices);
	}

	VulkanVertexBuffer::VulkanVertexBuffer(const uint32_t size, const void* data)
	{
		auto vertices = std::vector<Vertex>(size);
		auto totalSize = size * sizeof(Vertex);
		std::memcpy(vertices.data(), data, totalSize);
		createVertexBuffers(vertices);
		setData(totalSize, vertices.data());
	}

	VulkanVertexBuffer::~VulkanVertexBuffer()
	{
		const auto device = DeviceInstance.getDevice();
		vkDestroyBuffer(device, vertexBuffer, nullptr);
		vkFreeMemory(device, vertexMemory, nullptr);
	}

	void VulkanVertexBuffer::registerAttributes(const VertexElements& elements) const
	{
	}

	void VulkanVertexBuffer::setData(const uint32_t size, const void* data) const
	{
		const auto device = DeviceInstance.getDevice();
		
		void* dstData = nullptr;
		vkMapMemory(device, vertexMemory, 0, size, 0, &dstData);
		std::memcpy(dstData, data, size);
		vkUnmapMemory(device, vertexMemory);
	}

	void VulkanVertexBuffer::bind() const
	{
	}

	void VulkanVertexBuffer::unbind() const
	{
	}

	void VulkanVertexBuffer::bind(const VkCommandBuffer commandBuffer) const
	{
		auto buffers = std::array<VkBuffer, 1>{vertexBuffer};
		auto offsets = std::array<VkDeviceSize, 1>{0};
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers.data(), offsets.data());
	}

	void VulkanVertexBuffer::draw(const VkCommandBuffer commandBuffer) const
	{
		vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
	}

	void VulkanVertexBuffer::createVertexBuffers(const std::vector<Vertex>& vertices)
	{
		vertexCount = static_cast<uint32_t>(vertices.size());
		if (vertexCount < 3)
		{
			RT_LOG_ERROR("Vertex count must be at least 3: vertexCount = {}", vertexCount);
		}

		auto bufferSize = static_cast<VkDeviceSize>(sizeof(Vertex) * vertexCount);
		DeviceInstance.createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			vertexBuffer,
			vertexMemory);
	}

	std::vector<VkVertexInputBindingDescription> Vertex::getBindingDescriptions()
	{
		auto bindingDesc = std::vector<VkVertexInputBindingDescription>(1);
		bindingDesc[0].binding = 0;
		bindingDesc[0].stride = sizeof(Vertex);
		bindingDesc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDesc;
	}

	std::vector<VkVertexInputAttributeDescription> Vertex::getAttributeDescriptions()
	{
		auto attribDesc = std::vector<VkVertexInputAttributeDescription>(2);
		attribDesc[0].binding = 0;
		attribDesc[0].location = 0;
		attribDesc[0].format = VK_FORMAT_R32G32_SFLOAT;
		attribDesc[0].offset = offsetof(Vertex, position);
		
		attribDesc[1].binding = 0;
		attribDesc[1].location = 1;
		attribDesc[1].format = VK_FORMAT_R32G32_SFLOAT;
		attribDesc[1].offset = offsetof(Vertex, tex);
		return attribDesc;
	}


	VulkanUniform::VulkanUniform(const UniformType uniformType, const uint32_t instanceSize)
		: uniformType{uniformType}
	{
		alignedSize = calculateAlignedSize(
			instanceSize,
			UniformType::Uniform == uniformType ?
				DeviceInstance.getLimits().minUniformBufferOffsetAlignment :
				DeviceInstance.getLimits().minStorageBufferOffsetAlignment);
		
		DeviceInstance.createBuffer(
			alignedSize,
			uniformType2VkBuffBit(uniformType),
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			uniBuffer,
			uniMemory);

		vkMapMemory(DeviceInstance.getDevice(), uniMemory, 0, VK_WHOLE_SIZE, 0, &mapped);
	}

	VulkanUniform::VulkanUniform(const Texture& sampler, const uint32_t binding)
		: uniformType{UniformType::Sampler}
	{
		const auto& vulkanSampler = static_cast<const VulkanTexture&>(sampler);

		imgInfo = VkDescriptorImageInfo{};
		imgInfo.sampler = vulkanSampler.getSampler();
		imgInfo.imageView = vulkanSampler.getImageView();
		imgInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	VulkanUniform::~VulkanUniform()
	{
		if (UniformType::Sampler != uniformType)
		{
			const auto device = DeviceInstance.getDevice();
			vkUnmapMemory(device, uniMemory);
			vkDestroyBuffer(device, uniBuffer, nullptr);
			vkFreeMemory(device, uniMemory, nullptr);
		}
	}

	void VulkanUniform::bind(const uint32_t binding) const
	{
		bufferInfo = VkDescriptorBufferInfo{};
		bufferInfo.offset = 0;
		bufferInfo.range = VK_WHOLE_SIZE;
		bufferInfo.buffer = uniBuffer;
	}

	void VulkanUniform::setData(const void* data, const uint32_t size, const uint32_t offset)
	{
		void* dst = (uint8_t*)mapped + offset;
		std::memcpy(dst, data, size);

		uniformsToFlush.push_back(this);
	}

	void VulkanUniform::flush() const
	{
		auto memRange = VkMappedMemoryRange{};
		memRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		memRange.memory = uniMemory;
		memRange.offset = 0;
		memRange.size = VK_WHOLE_SIZE;
		RT_CORE_ASSERT(
			vkFlushMappedMemoryRanges(DeviceInstance.getDevice(), 1, &memRange) == VK_SUCCESS,
			"Failed to flush uniform buffer!");
	}

	constexpr VkBufferUsageFlagBits VulkanUniform::uniformType2VkBuffBit(const UniformType uniformType)
	{
		switch (uniformType)
		{
			case UniformType::Uniform: return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			case UniformType::Storage: return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		}
		return VkBufferUsageFlagBits{};
	}

	constexpr uint32_t VulkanUniform::calculateAlignedSize(const uint32_t initialSize, const uint32_t minAlignment)
	{
		return (initialSize + minAlignment - 1) & ~(minAlignment - 1);
	}

	std::vector<VulkanUniform*>& getUniformsToFlush()
	{
		return uniformsToFlush;
	}

}
