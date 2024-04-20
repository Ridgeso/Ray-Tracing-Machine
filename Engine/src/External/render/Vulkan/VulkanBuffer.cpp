#include "VulkanBuffer.h"

#include "Engine/Core/Log.h"
#include "Engine/Core/Assert.h"

#include "Swapchain.h"

namespace RT::Vulkan
{

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
		attribDesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attribDesc[1].offset = offsetof(Vertex, color);
		return attribDesc;
	}

	VulkanUniform::VulkanUniform(const uint32_t instanceSize, const uint32_t instanceCount)
		: alignedSize{instanceSize}
		, instanceCount{instanceCount}
	{
		DeviceInstance.createBuffer(
			alignedSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			uniBuffer,
			uniMemory);

		auto req = VkMemoryRequirements{};
		vkGetBufferMemoryRequirements(DeviceInstance.getDevice(), uniBuffer, &req);

		RT_LOG_TRACE("Uniform Requirements: {} {} {}", req.alignment, req.size, req.memoryTypeBits);
	}

	VulkanUniform::~VulkanUniform()
	{
		const auto device = DeviceInstance.getDevice();
		vkDestroyBuffer(device, uniBuffer, nullptr);
		vkFreeMemory(device, uniMemory, nullptr);
	}

	void VulkanUniform::setData(const void* data, uint32_t size)
	{
		void* mapped = nullptr;
		
		/*
		* TODO: VK_WHOLE_SIZE must be mutile of nonCoherentAtomSize (64 in my case)
		* probably are needed two sizes, one for the uniform size second fo the aligned size
		* 
		* Another solution is to use VK_MEMORY_PROPERTY_HOST_COHERENT_BIT in creation but
		* I want to keep manual flush for education purpose
		*/
		vkMapMemory(DeviceInstance.getDevice(), uniMemory, 0, VK_WHOLE_SIZE, 0, &mapped);

		std::memcpy(mapped, data, size);

		auto memRange = VkMappedMemoryRange{};
		memRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		memRange.memory = uniMemory;
		memRange.offset = 0;
		// As above
		memRange.size = VK_WHOLE_SIZE;

		RT_CORE_ASSERT(
			vkFlushMappedMemoryRanges(DeviceInstance.getDevice(), 1, &memRange) == VK_SUCCESS,
			"Failed to flush uniform buffer!");
		
		vkUnmapMemory(DeviceInstance.getDevice(), uniMemory);
	}

	VulkanStorage::VulkanStorage(const uint32_t instanceSize, const uint32_t instanceCount)
		: alignedSize{instanceSize}
		, instanceCount{instanceCount}
	{
		DeviceInstance.createBuffer(
			alignedSize,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			stoBuffer,
			stoMemory);

		auto req = VkMemoryRequirements{};
		vkGetBufferMemoryRequirements(DeviceInstance.getDevice(), stoBuffer, &req);

		RT_LOG_TRACE("Storage Requirements: {} {} {}", req.alignment, req.size, req.memoryTypeBits);
	}

	VulkanStorage::~VulkanStorage()
	{
		const auto device = DeviceInstance.getDevice();
		vkDestroyBuffer(device, stoBuffer, nullptr);
		vkFreeMemory(device, stoMemory, nullptr);
	}

	void VulkanStorage::setData(const void* data, uint32_t size)
	{
		void* mapped = nullptr;

		/*
		* TODO: Same case as in uniform
		*/
		vkMapMemory(DeviceInstance.getDevice(), stoMemory, 0, VK_WHOLE_SIZE, 0, &mapped);

		std::memcpy(mapped, data, size);

		auto memRange = VkMappedMemoryRange{};
		memRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		memRange.memory = stoMemory;
		memRange.offset = 0;
		memRange.size = VK_WHOLE_SIZE;

		RT_CORE_ASSERT(
			vkFlushMappedMemoryRanges(DeviceInstance.getDevice(), 1, &memRange) == VK_SUCCESS,
			"Failed to flush storage buffer!");

		vkUnmapMemory(DeviceInstance.getDevice(), stoMemory);
	}

}
