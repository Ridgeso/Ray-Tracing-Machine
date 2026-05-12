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
    class Semaphore
    {
    public:
        enum class Kind : uint8_t
        {
            Binary = 0,
            Timeline = 1,
        };

        static constexpr uint64_t noTimeout = std::numeric_limits<uint64_t>::max();

        explicit Semaphore(const Kind kind, const uint64_t initialValue = 0u);
        ~Semaphore() noexcept;

        void create(const Kind kind = Kind::Binary, const uint64_t initialValue = 0u);
        void destroy() noexcept;

        [[nodiscard]] VkSemaphore handle() const noexcept { return sem; }
        [[nodiscard]] Kind kind() const noexcept { return semKind; }

        bool wait(const uint64_t value, const uint64_t timeoutNs = noTimeout) const;
        bool wait(const uint64_t value, const std::chrono::nanoseconds timeout) const
        {
            return wait(value, static_cast<uint64_t>(timeout.count()));
        }

        void signal(const uint64_t value) const;
        [[nodiscard]] uint64_t value() const;

        static bool waitBatch(const VkSemaphore* handles, const uint64_t* values, const uint32_t count, const bool allSignaled = true, const uint64_t timeoutNs = noTimeout);
        static bool waitBatch(std::span<const Semaphore> sems, std::span<const uint64_t> values, const bool allSignaled = true, const uint64_t timeoutNs = noTimeout);

    private:
        VkSemaphore sem = VK_NULL_HANDLE;
        Kind semKind = Kind::Binary;
    };

}
