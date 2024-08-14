#include <numeric>
#include <algorithm>

#include "VulkanBuffer.h"

#include "utils/Debug.h"
#include "Swapchain.h"
#include "VulkanTexture.h"

namespace RT::Vulkan
{

	auto uniformsToFlush = std::vector<VulkanUniform*>();

	VulkanVertexBuffer::VulkanVertexBuffer(const uint32_t size)
	{
		RT_LOG_INFO("Creating VertexBuffer: {{ size = {} }}", size);
		auto vertices = std::vector<Vertex>(size);
		createVertexBuffers(vertices);
		RT_LOG_INFO("VertexBuffer created");
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
		RT_LOG_INFO("Creating Uniform: {{ type = {}, size = {} }}", RT::Utils::uniformType2Str(uniformType), instanceSize);

		const auto vkLimits = DeviceInstance.getLimits();
		const auto minAlignment = std::lcm(
			vkLimits.nonCoherentAtomSize,
			UniformType::Uniform == uniformType ?
				vkLimits.minUniformBufferOffsetAlignment :
				vkLimits.minStorageBufferOffsetAlignment);

		alignedSize = calculateAlignedSize(instanceSize, minAlignment);

		masterBuffer.resize(alignedSize);
		std::fill(masterBuffer.begin(), masterBuffer.end(), 0);

		DeviceInstance.createBuffer(
			wholeSize(),
			uniformType2VkBuffBit(uniformType),
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			uniBuffer,
			uniMemory);
		CHECK_VK(vkMapMemory(DeviceInstance.getDevice(), uniMemory, 0, wholeSize(), 0, &mapped), "faile to map Uniform memory!");

		descriptorInfo = std::array<VkDescriptorBufferInfo, Constants::MAX_FRAMES_IN_FLIGHT>{};
		for (uint32_t offsetIdx = 0u; offsetIdx < descriptorInfo.size(); offsetIdx++)
		{
			auto& bufferInfo = descriptorInfo[offsetIdx];
			bufferInfo.offset = offsetIdx * alignedSize;
			bufferInfo.range = alignedSize;
			bufferInfo.buffer = uniBuffer;
		}
		RT_LOG_INFO("Uniform created");
	}

	VulkanUniform::~VulkanUniform()
	{
		const auto device = DeviceInstance.getDevice();
		vkUnmapMemory(device, uniMemory);
		vkDestroyBuffer(device, uniBuffer, nullptr);
		vkFreeMemory(device, uniMemory, nullptr);
	}

	void VulkanUniform::setData(const void* data, const uint32_t size, const uint32_t offset)
	{
		auto* dst = masterBuffer.data() + offset;
		std::memcpy(dst, data, size);

		if (not stillNeedFlush())
		{
			uniformsToFlush.push_back(this);
		}
		std::fill(flashInThisFrame.begin(), flashInThisFrame.end(), true);
	}

	bool VulkanUniform::flush() const
	{
		const auto currFrame = SwapchainInstance->getCurrentFrame();
		if (not flashInThisFrame[currFrame])
		{
			return true;
		}

		copyToRegionBuff(currFrame);

		auto memRange = VkMappedMemoryRange{};
		memRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		memRange.memory = uniMemory;
		memRange.offset = alignedSize * currFrame;
		memRange.size = alignedSize;
		CHECK_VK(
			vkFlushMappedMemoryRanges(DeviceInstance.getDevice(), 1, &memRange),
			"Failed to flush uniform buffer!");
		
		flashInThisFrame[currFrame] = false;
		return stillNeedFlush();
	}

	bool VulkanUniform::stillNeedFlush() const
	{
		return std::any_of(flashInThisFrame.begin(), flashInThisFrame.end(), [](bool flashed) { return flashed; });
	}

	void VulkanUniform::copyToRegionBuff(const uint8_t buffIdx) const
	{
		auto dst = (uint8_t*)mapped + buffIdx * alignedSize;
		std::memcpy(dst, masterBuffer.data(), alignedSize);
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
