#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace RT::Vulkan
{

	inline constexpr uint32_t invalidImgIdx = 0xFFFFFFFF;

	struct Context
	{
	public:
		static inline uint32_t imgIdx = 0u;
		static inline VkCommandBuffer frameCmds = {};
	};

}
