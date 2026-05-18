#pragma once

#include <array>

#include <vulkan/vulkan.h>

#include "utils/Constants.h"

namespace RT::Vulkan
{

    class CommandBuffer
    {
        using CommandBuffers = std::array<VkCommandBuffer, Constants::MAX_FRAMES_IN_FLIGHT>;

    public:
        CommandBuffer() noexcept = default;
        ~CommandBuffer() noexcept = default;

        void create(VkCommandPool commandPool, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        void destroy() noexcept;

        void begin(uint32_t slotIdx, VkCommandBufferUsageFlags flags = 0) const;
        void end(uint32_t slotIdx) const;

        [[nodiscard]] VkCommandBuffer handle(uint32_t slotIdx) const noexcept { return cmdBuffers[slotIdx]; }

    private:
        CommandBuffers cmdBuffers = {};
        VkCommandPool pool = VK_NULL_HANDLE;
    };

}
