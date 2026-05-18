#include "RenderThread.h"

#include <array>
#include <limits>

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

#include "Device.h"
#include "Swapchain.h"
#include "utils/Debug.h"

namespace RT::Vulkan
{
namespace
{

    void recordGuiCmdBuffer(const VkCommandBuffer guiCmdBuff, const uint32_t imgIdx)
    {
        auto beginInfo = VkCommandBufferBeginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkResetCommandBuffer(guiCmdBuff, 0);
        CHECK_VK(vkBeginCommandBuffer(guiCmdBuff, &beginInfo), "failed to begin gui command buffer!");

        auto renderPassInfo = VkRenderPassBeginInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = SwapchainInstance->getRenderPass();
        renderPassInfo.framebuffer = SwapchainInstance->getFramebuffers()[imgIdx];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = SwapchainInstance->getWindowExtent();

        constexpr auto clearValues = std::array{VkClearValue{{ 0.0f, 0.0f, 0.0f, 1.0f }}};
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(guiCmdBuff, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        auto viewport = VkViewport{};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = static_cast<float>(SwapchainInstance->getSwapchainExtent().width);
        viewport.height = static_cast<float>(SwapchainInstance->getSwapchainExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        auto scissor = VkRect2D{};
        scissor.offset = { 0, 0 };
        scissor.extent = SwapchainInstance->getSwapchainExtent();
        vkCmdSetViewport(guiCmdBuff, 0, 1, &viewport);
        vkCmdSetScissor(guiCmdBuff, 0, 1, &scissor);

        auto* drawData = ImGui::GetDrawData();
        ImGui_ImplVulkan_RenderDrawData(drawData, guiCmdBuff);

        vkCmdEndRenderPass(guiCmdBuff);

        CHECK_VK(vkEndCommandBuffer(guiCmdBuff), "failed to record gui command buffer");
    }

} // namespace

    RenderThread::~RenderThread()
    {
        if (worker.joinable())
        {
            stopFlag = true;
            readyCv.notify_all();
            worker.join();
        }
    }

    void RenderThread::start(const CommandBuffers& mainCmdBuffs)
    {
        RT_ASSERT(not worker.joinable(), "RenderThread::start called twice");

        for (uint8_t i = 0; i < Constants::MAX_FRAMES_IN_FLIGHT; ++i)
        {
            slots[i].slotIdx = i;
            slots[i].mainCmdBuff = mainCmdBuffs[i];
            slots[i].imgIdx = 0u;
            freeSlots.push(&slots[i]);
        }

        imguiOutstanding = false;
        stopFlag = false;

        worker = std::thread{ &RenderThread::runLoop, this };
    }

    void RenderThread::stop()
    {
        if (not worker.joinable())
        {
            return;
        }

        stopFlag = true;
        readyCv.notify_all();

        worker.join();

        Slots{}.swap(freeSlots);
        Slots{}.swap(readySlots);
        imguiOutstanding = false;
    }

    void RenderThread::waitImGuiConsumed()
    {
        auto lock = std::unique_lock<std::mutex>{ imguiMtx };
        imguiCv.wait(lock, [&imguiOutstanding=imguiOutstanding]{ return not imguiOutstanding; });
    }

    FrameSlot* RenderThread::acquireFreeSlot()
    {
        auto lock = std::unique_lock<std::mutex>{ freeMtx };
        freeCv.wait(lock, [&freeSlots=freeSlots]{ return not freeSlots.empty(); });
        auto* slot = freeSlots.front();
        freeSlots.pop();
        return slot;
    }

    void RenderThread::drainAndPause()
    {
        if (not worker.joinable())
        {
            return;
        }
        auto lock = std::unique_lock<std::mutex>{ freeMtx };
        freeCv.wait(lock, [&freeSlots=freeSlots]{ return freeSlots.size() == Constants::MAX_FRAMES_IN_FLIGHT; });
    }

    void RenderThread::submitSlot(FrameSlot* slot, VkSemaphore extraWaitSem, VkSemaphore extraWaitSem2)
    {
        RT_ASSERT(slot != nullptr, "RenderThread::submitSlot with null slot");

        slot->extraWaitSem  = extraWaitSem;
        slot->extraWaitSem2 = extraWaitSem2;

        {
            auto lock = std::unique_lock<std::mutex>{ imguiMtx };
            imguiOutstanding = true;
        }

        {
            auto lock = std::unique_lock<std::mutex>{ readyMtx };
            readySlots.push(slot);
        }
        readyCv.notify_one();
    }

    void RenderThread::runLoop()
    {
        while (true)
        {
            FrameSlot* slot = nullptr;
            {
                auto lock = std::unique_lock<std::mutex>{ readyMtx };
                readyCv.wait(lock, [&]{ return stopFlag.load() or not readySlots.empty(); });
                if (stopFlag.load() and readySlots.empty())
                {
                    return;
                }
                slot = readySlots.front();
                readySlots.pop();
            }

            processSlot(slot);

            {
                auto lock = std::unique_lock<std::mutex>{ freeMtx };
                freeSlots.push(slot);
            }
            freeCv.notify_one();
        }
    }

    void RenderThread::processSlot(FrameSlot* slot)
    {
        const auto slotIdx = slot->slotIdx;

        auto acquireResult = SwapchainInstance->acquireNextImage(slot->imgIdx, slotIdx);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            {
                auto lock = std::unique_lock<std::mutex>{ imguiMtx };
                imguiOutstanding = false;
            }
            imguiCv.notify_one();
            return;
        }
        RT_ASSERT(acquireResult == VK_SUCCESS or acquireResult == VK_SUBOPTIMAL_KHR, "failed to acquire swap chain image");

        recordGuiCmdBuffer(slot->mainCmdBuff, slot->imgIdx);

        {
            auto lock = std::unique_lock<std::mutex>{ imguiMtx };
            imguiOutstanding = false;
        }
        imguiCv.notify_one();

        const auto extraWaitSem  = slot->extraWaitSem;
        const auto extraWaitSem2 = slot->extraWaitSem2;
        slot->extraWaitSem  = VK_NULL_HANDLE;
        slot->extraWaitSem2 = VK_NULL_HANDLE;

        const auto presentResult = SwapchainInstance->submitCommandBuffers(slot->mainCmdBuff, slot->imgIdx, slotIdx, extraWaitSem, extraWaitSem2);
        RT_ASSERT(presentResult == VK_SUCCESS or presentResult == VK_ERROR_OUT_OF_DATE_KHR or presentResult == VK_SUBOPTIMAL_KHR, "failed to present swap chain image");
    }

}
