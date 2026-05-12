#include "Semaphore.h"

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

    Semaphore::Semaphore(const Kind kind, const uint64_t initialValue)
    {
        create(kind, initialValue);
    }

    Semaphore::~Semaphore() noexcept
    {
        destroy();
    }

    void Semaphore::create(const Kind kind, const uint64_t initialValue)
    {
        RT_ASSERT(sem == VK_NULL_HANDLE, "Semaphore::create called on already-created semaphore");

        auto typeInfo = VkSemaphoreTypeCreateInfo{};
        typeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        typeInfo.semaphoreType = (kind == Kind::Timeline)
            ? VK_SEMAPHORE_TYPE_TIMELINE
            : VK_SEMAPHORE_TYPE_BINARY;
        typeInfo.initialValue = (kind == Kind::Timeline) ? initialValue : 0u;

        auto info = VkSemaphoreCreateInfo{};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        info.pNext = (kind == Kind::Timeline) ? &typeInfo : nullptr;

        CHECK_VK(
            vkCreateSemaphore(DeviceInstance.getDevice(), &info, nullptr, &sem),
            "failed to create semaphore!");

        semKind = kind;
    }

    void Semaphore::destroy() noexcept
    {
        if (sem != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(DeviceInstance.getDevice(), sem, nullptr);
            sem = VK_NULL_HANDLE;
        }
        semKind = Kind::Binary;
    }

    bool Semaphore::wait(const uint64_t value, const uint64_t timeoutNs) const
    {
        RT_ASSERT(sem != VK_NULL_HANDLE, "Semaphore::wait on invalid semaphore");
        RT_ASSERT(semKind == Kind::Timeline, "Semaphore::wait requires a Timeline semaphore");

        auto waitInfo = VkSemaphoreWaitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.semaphoreCount = 1u;
        waitInfo.pSemaphores = &sem;
        waitInfo.pValues = &value;

        const auto result = vkWaitSemaphores(DeviceInstance.getDevice(), &waitInfo, timeoutNs);
        if (result == VK_SUCCESS) return true;
        if (result == VK_TIMEOUT) return false;
        RT_ASSERT(false, "vkWaitSemaphores failed");
        return false;
    }

    void Semaphore::signal(const uint64_t value) const
    {
        RT_ASSERT(sem != VK_NULL_HANDLE, "Semaphore::signal on invalid semaphore");
        RT_ASSERT(semKind == Kind::Timeline, "Semaphore::signal requires a Timeline semaphore");

        auto signalInfo = VkSemaphoreSignalInfo{};
        signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
        signalInfo.semaphore = sem;
        signalInfo.value = value;

        CHECK_VK(vkSignalSemaphore(DeviceInstance.getDevice(), &signalInfo), "failed to signal semaphore!");
    }

    uint64_t Semaphore::value() const
    {
        RT_ASSERT(sem != VK_NULL_HANDLE, "Semaphore::value on invalid semaphore");
        RT_ASSERT(semKind == Kind::Timeline, "Semaphore::value requires a Timeline semaphore");

        uint64_t v = 0u;
        CHECK_VK(vkGetSemaphoreCounterValue(DeviceInstance.getDevice(), sem, &v), "failed to read semaphore value!");
        return v;
    }

    bool Semaphore::waitBatch(const VkSemaphore* handles, const uint64_t* values, const uint32_t count, const bool allSignaled, const uint64_t timeoutNs)
    {
        if (count == 0)
        {
            return true;
        }

        auto info = VkSemaphoreWaitInfo{};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        info.flags = allSignaled ? 0 : VK_SEMAPHORE_WAIT_ANY_BIT;
        info.semaphoreCount = count;
        info.pSemaphores = handles;
        info.pValues = values;

        const auto result = vkWaitSemaphores(DeviceInstance.getDevice(), &info, timeoutNs);
        if (result == VK_SUCCESS) return true;
        if (result == VK_TIMEOUT) return false;
        RT_ASSERT(false, "vkWaitSemaphores (waitBatch) failed");
        return false;
    }

    bool Semaphore::waitBatch(std::span<const Semaphore> sems, std::span<const uint64_t> values, const bool allSignaled, const uint64_t timeoutNs)
    {
        RT_ASSERT(sems.size() == values.size(), "Semaphore::waitBatch size mismatch between semaphores and values");
        const auto count = static_cast<uint32_t>(sems.size());
        if (count <= batchSize)
        {
            std::array<VkSemaphore, batchSize> stackHandles{};
            for (uint32_t i = 0; i < count; ++i)
            {
                RT_ASSERT(sems[i].semKind == Kind::Timeline, "Semaphore::waitBatch requires Timeline semaphores");
                stackHandles[i] = sems[i].sem;
            }
            return waitBatch(stackHandles.data(), values.data(), count, allSignaled, timeoutNs);
        }
        auto heapHandles = std::vector<VkSemaphore>{};
        heapHandles.reserve(count);
        for (const auto& s : sems)
        {
            RT_ASSERT(s.semKind == Kind::Timeline, "Semaphore::waitBatch requires Timeline semaphores");
            heapHandles.push_back(s.sem);
        }
        return waitBatch(heapHandles.data(), values.data(), count, allSignaled, timeoutNs);
    }

} // namespace RT::Vulkan
