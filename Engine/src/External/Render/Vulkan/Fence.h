#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <span>

#include <vulkan/vulkan.h>

namespace RT::Vulkan
{
    class Fence
    {
    public:
        enum class State : uint8_t
        {
            Unsignaled = 0,
            Signaled = 1,
        };

        static constexpr uint64_t noTimeout = std::numeric_limits<uint64_t>::max();

        Fence() noexcept = default;
        explicit Fence(const State initial);
        ~Fence() noexcept;

        void create(const State initial = State::Unsignaled);
        void destroy() noexcept;
        void reset();

        [[nodiscard]] bool isSignaled() const;
        [[nodiscard]] VkFence handle() const noexcept { return fence; }

        bool wait(const uint64_t timeoutNs = noTimeout) const;
        bool wait(const std::chrono::nanoseconds timeout) const
        {
            return wait(static_cast<uint64_t>(timeout.count()));
        }

        static bool waitBatch(const VkFence* handles, const uint32_t count, const bool allSignaled = true, const uint64_t timeoutNs = noTimeout);
        static void resetBatch(const VkFence* handles, const uint32_t count);

        static bool waitBatch(std::span<const Fence> fences, const bool allSignaled = true, const uint64_t timeoutNs = noTimeout);
        static void resetBatch(std::span<const Fence> fences);

    private:
        VkFence fence = VK_NULL_HANDLE;
    };

}
