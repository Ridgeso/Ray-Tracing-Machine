#include "Fence.h"

#include <array>
#include <vector>

#include "Device.h"
#include "utils/Debug.h"

namespace RT::Vulkan
{
namespace
{
    constexpr uint32_t batchSize = 16u;
} // namespace

    Fence::Fence(const State initial)
    {
        create(initial);
    }

    Fence::~Fence() noexcept
    {
        destroy();
    }

    void Fence::create(const State initial)
    {
        RT_ASSERT(fence == VK_NULL_HANDLE, "Fence::create called on already-created fence");

        auto info = VkFenceCreateInfo{};
        info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        info.flags = (initial == State::Signaled) ? VK_FENCE_CREATE_SIGNALED_BIT : 0u;

        CHECK_VK(
            vkCreateFence(DeviceInstance.getDevice(), &info, nullptr, &fence),
            "failed to create fence!");
    }

    void Fence::destroy() noexcept
    {
        if (fence != VK_NULL_HANDLE)
        {
            vkDestroyFence(DeviceInstance.getDevice(), fence, nullptr);
            fence = VK_NULL_HANDLE;
        }
    }

    void Fence::reset()
    {
        RT_ASSERT(fence != VK_NULL_HANDLE, "Fence::reset on invalid fence");
        CHECK_VK(vkResetFences(DeviceInstance.getDevice(), 1, &fence), "failed to reset fence!");
    }

    bool Fence::isSignaled() const
    {
        const auto status = vkGetFenceStatus(DeviceInstance.getDevice(), fence);
        if (status == VK_SUCCESS)
        {
            return true;
        }
        if (status == VK_NOT_READY)
        {
            return false;
        }
        RT_ASSERT(false, "vkGetFenceStatus failed");
        return false;
    }

    bool Fence::wait(const uint64_t timeoutNs) const
    {
        RT_ASSERT(fence != VK_NULL_HANDLE, "Fence::wait on invalid fence");
        const auto result = vkWaitForFences(DeviceInstance.getDevice(), 1, &fence, VK_TRUE, timeoutNs);
        if (result == VK_SUCCESS)
        {
            return true;
        }
        if (result == VK_TIMEOUT)
        {
            return false;
        }
        RT_ASSERT(false, "vkWaitForFences failed");
        return false;
    }

    bool Fence::waitBatch(const VkFence* handles, const uint32_t count, const bool allSignaled, const uint64_t timeoutNs)
    {
        if (count == 0)
        {
            return true;
        }
        const auto result = vkWaitForFences(DeviceInstance.getDevice(), count, handles, allSignaled, timeoutNs);
        if (result == VK_SUCCESS)
        {
            return true;
        }
        if (result == VK_TIMEOUT)
        {
            return false;
        }
        RT_ASSERT(false, "vkWaitForFences ({}) failed", allSignaled ? "allSignaled" : "anySignaled");
        return false;
    }

    void Fence::resetBatch(const VkFence* handles, const uint32_t count)
    {
        if (count == 0)
        {
            return;
        }
        CHECK_VK(vkResetFences(DeviceInstance.getDevice(), count, handles), "failed to reset fences (batch)!");
    }

    bool Fence::waitBatch(std::span<const Fence> fences, const bool allSignaled, const uint64_t timeoutNs)
    {
        const auto count = static_cast<uint32_t>(fences.size());
        if (count <= batchSize)
        {
            std::array<VkFence, batchSize> stackHandles{};
            for (uint32_t i = 0; i < count; ++i)
            {
                stackHandles[i] = fences[i].fence;
            }
            return waitBatch(stackHandles.data(), count, allSignaled, timeoutNs);
        }
        auto heapHandles = std::vector<VkFence>{};
        heapHandles.reserve(count);
        for (const auto& f : fences)
        {
            heapHandles.push_back(f.fence);
        }
        return waitBatch(heapHandles.data(), count, allSignaled, timeoutNs);
    }
    
    void Fence::resetBatch(std::span<const Fence> fences)
    {
        const auto count = static_cast<uint32_t>(fences.size());
        if (count <= batchSize)
        {
            std::array<VkFence, batchSize> stackHandles{};
            for (uint32_t i = 0; i < count; ++i)
            {
                stackHandles[i] = fences[i].fence;
            }
            resetBatch(stackHandles.data(), count);
            return;
        }
        auto heapHandles = std::vector<VkFence>{};
        heapHandles.reserve(count);
        for (const auto& f : fences)
        {
            heapHandles.push_back(f.fence);
        }
        resetBatch(heapHandles.data(), count);
    }

} // namespace RT::Vulkan
