#include "RenderBase.h"
#include "Buffer.h"

#include "External/Render/OpenGl/OpenGlBuffer.h"
#include "External/Render/Vulkan/VulkanBuffer.h"

namespace RT
{

	Local<VertexBuffer> VertexBuffer::create(const uint32_t size)
	{
		switch (GlobalRenderAPI)
		{
			// case RenderAPI::OpenGL: return makeLocal<OpenGl::OpenGlVertexBuffer>(size);
			case RenderAPI::Vulkan: return makeLocal<Vulkan::VulkanVertexBuffer>(size);
		}
		return nullptr;
	}

	Local<VertexBuffer> VertexBuffer::create(const uint32_t size, const void* data)
	{
		switch (GlobalRenderAPI)
		{
			// case RenderAPI::OpenGL: return makeLocal<OpenGl::OpenGlVertexBuffer>(size, data);
			case RenderAPI::Vulkan: return makeLocal<Vulkan::VulkanVertexBuffer>(size, data);
		}
		return nullptr;
	}

	VertexBuffer::~VertexBuffer() {}

	Local<Uniform> Uniform::create(const UniformType uniformType, const uint32_t size)
	{
		switch (GlobalRenderAPI)
		{
			// case RenderAPI::OpenGL: return makeLocal<OpenGl::OpenGlUniform>(uniformType, size); break;
			case RenderAPI::Vulkan: return makeLocal<Vulkan::VulkanUniform>(uniformType, size); break;
		}
		return nullptr;
	}
	
	Local<Uniform> Uniform::create(const Texture& sampler, const uint32_t binding)
	{
		switch (GlobalRenderAPI)
		{
			// case RenderAPI::OpenGL: return makeLocal<OpenGl::OpenGlUniform>(sampler, binding); break;
			case RenderAPI::Vulkan: return makeLocal<Vulkan::VulkanUniform>(sampler, binding); break;
		}
		return nullptr;
	}

	Uniform::~Uniform() {}

}
