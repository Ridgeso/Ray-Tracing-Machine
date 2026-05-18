#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <thread>

#include <vulkan/vulkan.h>

#include "CommandBuffer.h"

namespace RT::Vulkan
{

    struct FrameSlot
    {
        uint8_t slotIdx = 0u;
        VkCommandBuffer mainCmdBuff = VK_NULL_HANDLE;
        uint32_t imgIdx = 0u;
        VkSemaphore extraWaitSem  = VK_NULL_HANDLE;
        VkSemaphore extraWaitSem2 = VK_NULL_HANDLE;
    };

    class RenderThread
    {
        using Slots = std::queue<FrameSlot*>;

    public:
        RenderThread() = default;
        ~RenderThread();

        RenderThread(const RenderThread&) = delete;
        RenderThread(RenderThread&&) = delete;
        RenderThread& operator=(const RenderThread&) = delete;
        RenderThread& operator=(RenderThread&&) = delete;

        void start(const CommandBuffer& mainCmdBuffs);

        void stop();

        void waitImGuiConsumed();
        FrameSlot* acquireFreeSlot();
        void submitSlot(FrameSlot* slot, VkSemaphore extraWaitSem = VK_NULL_HANDLE, VkSemaphore extraWaitSem2 = VK_NULL_HANDLE);

        void drainAndPause();

    private:
        void runLoop();
        void processSlot(FrameSlot* slot);

        std::array<FrameSlot, Constants::MAX_FRAMES_IN_FLIGHT> slots = {};

        std::mutex freeMtx = {};
        std::condition_variable freeCv = {};
        Slots freeSlots = {};

        std::mutex readyMtx = {};
        std::condition_variable readyCv = {};
        Slots readySlots = {};

        std::mutex imguiMtx = {};
        std::condition_variable imguiCv = {};
        bool imguiOutstanding = false;

        std::thread worker = {};
        std::atomic<bool> stopFlag = false;
    };

}
