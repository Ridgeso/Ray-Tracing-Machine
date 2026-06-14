#include "RenderRecorder.h"
#include "External/Render/Vulkan/Context.h"
#include "External/Render/Vulkan/Device.h"
#include "External/Render/Vulkan/Fence.h"
#include "External/Render/Vulkan/Semaphore.h"
#include "External/Render/Vulkan/Utils/Debug.h"

namespace RT::Vulkan
{

    void RenderRecorder::init(VkQueue queue_, VkCommandPool commandPool)
    {
        queue = queue_;
        cmdBuffer = DeviceInstance.createCommandBuffer(commandPool);
        for (int32_t i = 0; i < Constants::MAX_FRAMES_IN_FLIGHT; i++)
        {
            fences[i].create(Fence::State::Signaled);
            finishedSemaphores[i].create(Semaphore::Kind::Binary);
        }
        slotIdx = 0u;
    }

    void RenderRecorder::shutdown()
    {
        cmdBuffer.destroy();
        for (int32_t i = 0; i < Constants::MAX_FRAMES_IN_FLIGHT; i++)
        {
            fences[i].destroy();
            finishedSemaphores[i].destroy();
        }
    }

    bool RenderRecorder::begin()
    {
		RT_ASSERT(Context::frameCmd == VK_NULL_HANDLE, "VulkanRenderApi::beginFrame called while a command buffer is already recording");

        if (not fences[slotIdx].isSignaled())
        {
            return false;
        }
        fences[slotIdx].reset();

        // fences[slotIdx].wait();
        // fences[slotIdx].reset();

		Context::frameCmd = cmdBuffer.handle(slotIdx);
		Context::slotIdx = slotIdx;
		cmdBuffer.begin(slotIdx);
    }

    void RenderRecorder::end()
    {
        RT_ASSERT(Context::frameCmd != VK_NULL_HANDLE, "VulkanRenderApi::endFrame called without beginFrame");

		cmdBuffer.end(slotIdx);
    }

    VkSemaphore RenderRecorder::submit()
    {
		const auto cmdHandle = cmdBuffer.handle(slotIdx);

		auto submitInfo = VkSubmitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1u;
		submitInfo.pCommandBuffers = &cmdHandle;

		const auto signalSemaphores = std::array{ finishedSemaphores[slotIdx].handle() };
		submitInfo.signalSemaphoreCount = signalSemaphores.size();
		submitInfo.pSignalSemaphores = signalSemaphores.data();

		const auto& deviceInstance = DeviceInstance;
		CHECK_VK(
			deviceInstance.queueSubmit(queue, 1, &submitInfo, fences[slotIdx].handle()),
			"failed to submit user-graphics command buffer!");

		Context::frameCmd = VK_NULL_HANDLE;
		Context::slotIdx = invalidSlotIdx;

        const auto pendingSemaphore = finishedSemaphores[slotIdx].handle();

        slotIdx = (slotIdx + 1u) % Constants::MAX_FRAMES_IN_FLIGHT;

        return pendingSemaphore;
    }
    
}
