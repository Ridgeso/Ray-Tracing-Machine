#pragma once

#include "External/Render/Vulkan/CommandBuffer.h"
#include "External/Render/Vulkan/Fence.h"
#include "External/Render/Vulkan/Semaphore.h"

namespace RT::Vulkan
{

    class RenderRecorder
    {
    public:
        RenderRecorder() noexcept = default;
        ~RenderRecorder() noexcept = default;

		void init(VkCommandPool commandPool);
		void shutdown();

        void begin();
		void end();
        VkSemaphore submit();

    private:
		CommandBuffer cmdBuffer = {};
        
		Fences fences = {};
		Semaphores finishedSemaphores = {};
		uint8_t slotIdx = 0u;
    };

}
