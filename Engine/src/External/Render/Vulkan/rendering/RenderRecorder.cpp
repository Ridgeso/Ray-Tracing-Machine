#include "RenderRecorder.h"
#include "External/Render/Vulkan/Context.h"
#include "External/Render/Vulkan/Device.h"
#include "External/Render/Vulkan/Fence.h"
#include "External/Render/Vulkan/Semaphore.h"
#include "External/Render/Vulkan/Utils/Debug.h"

namespace RT::Vulkan
{

    void RenderRecorder::init(VkQueue queue_, VkCommandPool commandPool, bool isInternal_)
    {
        queue = queue_;
        cmdBuffer = DeviceInstance.createCommandBuffer(commandPool);
        for (int32_t i = 0; i < Constants::MAX_FRAMES_IN_FLIGHT; i++)
        {
            fences[i].create(Fence::State::Signaled);
            finishedSemaphores[i].create(Semaphore::Kind::Binary);
        }
        slotIdx = 0u;

        isInternal = isInternal_;
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
		ASSERT("VULKAN", Context::frameCmd == VK_NULL_HANDLE, "VulkanRenderApi::beginFrame called while a command buffer is already recording");

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

        return true;
    }

    void RenderRecorder::end()
    {
        ASSERT("VULKAN", Context::frameCmd != VK_NULL_HANDLE, "VulkanRenderApi::endFrame called without beginFrame");

		cmdBuffer.end(slotIdx);
    }

    void RenderRecorder::submit()
    {
		const auto cmdHandle = cmdBuffer.handle(slotIdx);

		auto submitInfo = VkSubmitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1u;
		submitInfo.pCommandBuffers = &cmdHandle;

		const auto signalSemaphores = std::array{ finishedSemaphores[slotIdx].handle() };
		submitInfo.signalSemaphoreCount = signalSemaphores.size();
		submitInfo.pSignalSemaphores = signalSemaphores.data();

        auto waitSemaphores = std::array{ (VkSemaphore)VK_NULL_HANDLE };
        auto waitStages = std::array<VkPipelineStageFlags, 1>{ VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT };
        if (isInternal and waitSemaphore != VK_NULL_HANDLE)
        {
            uint32_t waitCount = 0u;
            waitSemaphores[waitCount++] = waitSemaphore;
            submitInfo.waitSemaphoreCount = waitCount;
            submitInfo.pWaitSemaphores = waitSemaphores.data();
            submitInfo.pWaitDstStageMask = waitStages.data();
        }

		CHECK_VK(
			DeviceInstance.queueSubmit(queue, 1, &submitInfo, fences[slotIdx].handle()),
			"failed to submit user-graphics command buffer!");

		Context::frameCmd = VK_NULL_HANDLE;
		Context::slotIdx = invalidSlotIdx;

        waitSemaphore = finishedSemaphores[slotIdx].handle();

        slotIdx = (slotIdx + 1u) % Constants::MAX_FRAMES_IN_FLIGHT;
    }

    VkSemaphore RenderRecorder::getWaitSemaphore() const
    {
        return isInternal ? VK_NULL_HANDLE : waitSemaphore;
    }
    
}
