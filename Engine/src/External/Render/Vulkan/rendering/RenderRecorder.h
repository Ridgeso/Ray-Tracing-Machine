#pragma once

#include "External/Render/Vulkan/CommandBuffer.h"
#include "External/Render/Vulkan/Fence.h"
#include "External/Render/Vulkan/Semaphore.h"

namespace RT::Vulkan
{

    class RenderRecorder
    {
    public:
		void init(VkQueue queue_, VkCommandPool commandPool, bool isInternal_ = false);
		void shutdown();

        bool begin();
		void end();
        void submit();

        VkSemaphore getWaitSemaphore() const;

    private:
        VkQueue queue = {};

		CommandBuffer cmdBuffer = {};
        
		Fences fences = {};
		Semaphores finishedSemaphores = {};
		uint8_t slotIdx = 0u;

        VkSemaphore waitSemaphore = VK_NULL_HANDLE;
        bool isInternal = false;
    };

}
