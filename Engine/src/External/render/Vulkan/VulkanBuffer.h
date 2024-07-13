#pragma once
#include <vector>

#include "Engine/Render/Buffer.h"

#include "Device.h"

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

	class VulkanVertexBuffer : public VertexBuffer
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

		void bind() const final;
		void unbind() const final;

		void bind(const VkCommandBuffer commandBuffer) const;
		void draw(const VkCommandBuffer commandBuffer) const;

	private:
		void createVertexBuffers(const std::vector<Vertex>& vertices);

	private:
		VkBuffer vertexBuffer = {};
		VkDeviceMemory vertexMemory = {};
		uint32_t vertexCount = 0u;
	};

	class VulkanUniform : public Uniform
	{
	public:
		VulkanUniform(const UniformType uniformType, const uint32_t instanceSize);
		VulkanUniform(const Texture& sampler, const uint32_t binding, const UniformType samplerType);
		~VulkanUniform() final;

		VulkanUniform(const VulkanUniform&) = delete;
		VulkanUniform(VulkanUniform&&) = delete;
		VulkanUniform& operator=(const VulkanUniform&) = delete;
		VulkanUniform&& operator=(VulkanUniform&&) = delete;

		void bind(const uint32_t binding) const final;
		void setData(const void* data, const uint32_t size, const uint32_t offset = 0u) final;
		
		void flush() const;

		VkBuffer getBuffer() const { return uniBuffer; }

		friend class VulkanDescriptor;

	private:
		static constexpr VkBufferUsageFlagBits uniformType2VkBuffBit(const UniformType uniformType);
		static constexpr uint32_t calculateAlignedSize(const uint32_t initialSize, const uint32_t minAlignment);

	private:
		UniformType uniformType = UniformType::None;
		uint32_t alignedSize = 0u;

		VkBuffer uniBuffer = {};
		VkDeviceMemory uniMemory = {};
		void* mapped = nullptr;

		mutable VkDescriptorBufferInfo bufferInfo = {};
		mutable VkDescriptorImageInfo imgInfo = {};
	};

	class MockVulkanUniform : public Uniform
	{
	public:
		MockVulkanUniform(const UniformType uniformType, const uint32_t instanceSize) {}
		MockVulkanUniform(const Texture& sampler, const uint32_t binding) {}
		~MockVulkanUniform() final {}

		void bind(const uint32_t binding) const final {}
		void setData(const void* data, const uint32_t size, const uint32_t offset = 0u) final {}
	};

	std::vector<VulkanUniform*>& getUniformsToFlush();

}
