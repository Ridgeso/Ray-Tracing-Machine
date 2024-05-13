#include "RenderBase.h"
#include "Descriptors.h"

#include "External/render/Vulkan/VulkanDescriptors.h"

namespace RT
{

	Local<DescriptorLayout> DescriptorLayout::create(const DescriptorLayoutSpec& spec)
	{
		switch (GlobalRenderAPI)
		{
			// case RenderAPI::OpenGL: return nullptr;
			case RenderAPI::Vulkan: return makeLocal<Vulkan::VulkanDescriptorLayout>(spec);
		}
		return nullptr;
	}

	DescriptorLayout::~DescriptorLayout() { }

	Local<DescriptorPool> DescriptorPool::create(const DescriptorPoolSpec& spec)
	{
		switch (GlobalRenderAPI)
		{
			// case RenderAPI::OpenGL: return nullptr;
			case RenderAPI::Vulkan: return makeLocal<Vulkan::VulkanDescriptorPool>(spec);
		}
		return nullptr;
	}

	DescriptorPool::~DescriptorPool() { }

	Local<DescriptorSet> DescriptorSet::create(const DescriptorLayout& layout, const DescriptorPool& pool)
	{
		switch (GlobalRenderAPI)
		{
			// case RenderAPI::OpenGL: return nullptr;
			case RenderAPI::Vulkan: return makeLocal<Vulkan::VulkanDescriptorSet>(layout, pool);
		}
		return nullptr;
	}
	
	DescriptorSet::~DescriptorSet() { }

}
