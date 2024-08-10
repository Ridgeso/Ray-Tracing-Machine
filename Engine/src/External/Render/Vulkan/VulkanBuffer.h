#pragma once
#include <array>
#include <vector>

#include "Engine/Render/Buffer.h"

#include "Device.h"
#include "utils/Constants.h"

#define GLM_FORCE_RADAINS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace RT::Vulkan
{

	struct Vertex
	{
		glm::vec2 position;
		glm::vec2 tex;

		static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
		static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
	};

	class VulkanVertexBuffer final : public VertexBuffer
	{
	public:
		VulkanVertexBuffer(const uint32_t size);
		VulkanVertexBuffer(const uint32_t size, const void* data);
		~VulkanVertexBuffer() final;
		
		VulkanVertexBuffer(const VulkanVertexBuffer&) = delete;
		VulkanVertexBuffer(VulkanVertexBuffer&&) = delete;
		VulkanVertexBuffer& operator=(const VulkanVertexBuffer&) = delete;
		VulkanVertexBuffer&& operator=(VulkanVertexBuffer&&) = delete;

		void registerAttributes(const VertexElements& elements) const final;
		void setData(const uint32_t size, const void* data) const final;
		const int32_t getCount() const final { return 0; }

		void bind(const VkCommandBuffer commandBuffer) const;
		void draw(const VkCommandBuffer commandBuffer) const;

	private:
		void createVertexBuffers(const std::vector<Vertex>& vertices);

	private:
		VkBuffer vertexBuffer = {};
		VkDeviceMemory vertexMemory = {};
		uint32_t vertexCount = 0u;
	};

	class VulkanUniform final : public Uniform
	{
		friend class Descriptors;

	public:
		VulkanUniform(const UniformType uniformType, const uint32_t instanceSize);
		~VulkanUniform() final;

		VulkanUniform(const VulkanUniform&) = delete;
		VulkanUniform(VulkanUniform&&) = delete;
		VulkanUniform& operator=(const VulkanUniform&) = delete;
		VulkanUniform&& operator=(VulkanUniform&&) = delete;

		void setData(const void* data, const uint32_t size, const uint32_t offset = 0u) final;

		bool flush() const;
		const VkDescriptorBufferInfo* getWriteBufferInfo(const uint32_t buffNr) const
		{
			return descriptorInfo.data() + buffNr;
		}

		VkBuffer getBuffer() const { return uniBuffer; }

	private:
		const uint32_t wholeSize() const { return alignedSize * Constants::MAX_FRAMES_IN_FLIGHT; }

		bool stillNeedFlush() const;
		void copyToRegionBuff(const uint8_t buffIdx) const;

		static constexpr VkBufferUsageFlagBits uniformType2VkBuffBit(const UniformType uniformType);
		static constexpr uint32_t calculateAlignedSize(const uint32_t initialSize, const uint32_t minAlignment);

	private:
		UniformType uniformType = UniformType::None;
		uint32_t alignedSize = 0u;
		std::vector<uint8_t> masterBuffer = {};

		VkBuffer uniBuffer = {};
		VkDeviceMemory uniMemory = {};
		void* mapped = nullptr;
		
		std::array<VkDescriptorBufferInfo, Constants::MAX_FRAMES_IN_FLIGHT> descriptorInfo = {};
		mutable std::array<bool, Constants::MAX_FRAMES_IN_FLIGHT> flashInThisFrame = {};
		
		//VkDescriptorImageInfo imgInfo;
	};

	std::vector<VulkanUniform*>& getUniformsToFlush();

}
