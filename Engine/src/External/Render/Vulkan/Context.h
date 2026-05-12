#pragma once

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

namespace RT::Vulkan
{

	inline constexpr uint8_t invalidSlotIdx = 0xFFu;

	struct Context
	{
	public:
		static inline uint8_t slotIdx = invalidSlotIdx;
		static inline VkCommandBuffer frameCmd = {};
	};

}
