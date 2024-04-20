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
		glm::vec3 color;

		static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
		static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
	};

	class VulkanVertexBuffer : public VertexBuffer
	{
	public:
		VulkanVertexBuffer(const uint32_t size);
		VulkanVertexBuffer(const uint32_t size, const void* data);
		~VulkanVertexBuffer() final;

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

	class VulkanUniform
	{
	public:
		VulkanUniform(const uint32_t instanceSize, const uint32_t instanceCount = 1u);
		~VulkanUniform();

		VulkanUniform(const VulkanUniform&) = delete;
		VulkanUniform(VulkanUniform&&) = delete;
		VulkanUniform& operator=(const VulkanUniform&) = delete;
		VulkanUniform&& operator=(VulkanUniform&&) = delete;

		void setData(const void* data, uint32_t size);
		VkBuffer getBuffer() const { return uniBuffer; }

	private:
		VkBuffer uniBuffer = {};
		VkDeviceMemory uniMemory = {};
		uint32_t alignedSize = 0u;
		uint32_t instanceCount = 1u;
	};

	class VulkanStorage
	{
	public:
		VulkanStorage(const uint32_t instanceSize, const uint32_t instanceCount = 1u);
		~VulkanStorage();

		VulkanStorage(const VulkanStorage&) = delete;
		VulkanStorage(VulkanStorage&&) = delete;
		VulkanStorage& operator=(const VulkanStorage&) = delete;
		VulkanStorage&& operator=(VulkanStorage&&) = delete;

		void setData(const void* data, uint32_t size);
		VkBuffer getBuffer() const { return stoBuffer; }

	private:
		VkBuffer stoBuffer = {};
		VkDeviceMemory stoMemory = {};
		uint32_t alignedSize = 0u;
		uint32_t instanceCount = 1u;
	};

}
