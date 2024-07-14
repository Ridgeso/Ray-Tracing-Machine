#include "RenderApi.h"
#include "Buffer.h"

#include "External/Render/OpenGl/OpenGlBuffer.h"
#include "External/Render/Vulkan/VulkanBuffer.h"

namespace RT
{

	Local<VertexBuffer> VertexBuffer::create(const uint32_t size)
	{
		switch (RenderApi::api)
		{
			// case RenderApi::Api::OpenGL: return makeLocal<OpenGl::OpenGlVertexBuffer>(size);
			case RenderApi::Api::Vulkan: return makeLocal<Vulkan::VulkanVertexBuffer>(size);
		}
		return nullptr;
	}

	Local<VertexBuffer> VertexBuffer::create(const uint32_t size, const void* data)
	{
		switch (RenderApi::api)
		{
			// case RenderApi::Api::OpenGL: return makeLocal<OpenGl::OpenGlVertexBuffer>(size, data);
			case RenderApi::Api::Vulkan: return makeLocal<Vulkan::VulkanVertexBuffer>(size, data);
		}
		return nullptr;
	}

	VertexBuffer::~VertexBuffer() {}

	Local<Uniform> Uniform::create(const UniformType uniformType, const uint32_t size)
	{
		switch (RenderApi::api)
		{
			// case RenderApi::Api::OpenGL: return makeLocal<OpenGl::OpenGlUniform>(uniformType, size); break;
			case RenderApi::Api::Vulkan: return makeLocal<Vulkan::VulkanUniform>(uniformType, size); break;
		}
		return nullptr;
	}
	
	Local<Uniform> Uniform::create(const Texture& sampler, const uint32_t binding, const UniformType samplerType)
	{
		switch (RenderApi::api)
		{
			// case RenderApi::Api::OpenGL: return makeLocal<OpenGl::OpenGlUniform>(sampler, binding); break;
			case RenderApi::Api::Vulkan: return makeLocal<Vulkan::VulkanUniform>(sampler, binding, samplerType); break;
		}
		return nullptr;
	}

	Uniform::~Uniform() {}

}
