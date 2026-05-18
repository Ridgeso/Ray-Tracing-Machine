#include "CommandBuffer.h"

#include "Device.h"
#include "utils/Debug.h"

namespace RT::Vulkan
{

    void CommandBuffer::create(VkCommandPool commandPool, VkCommandBufferLevel level)
    {
        pool = commandPool;

        auto allocInfo = VkCommandBufferAllocateInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = pool;
        allocInfo.level              = level;
        allocInfo.commandBufferCount = Constants::MAX_FRAMES_IN_FLIGHT;

        CHECK_VK(
            vkAllocateCommandBuffers(DeviceInstance.getDevice(), &allocInfo, cmdBuffers.data()),
            "failed to allocate command buffer!");
    }

    void CommandBuffer::destroy() noexcept
    {
        vkFreeCommandBuffers(DeviceInstance.getDevice(), pool, Constants::MAX_FRAMES_IN_FLIGHT, cmdBuffers.data());
        cmdBuffers.fill(VK_NULL_HANDLE);
    }

    void CommandBuffer::begin(uint32_t slotIdx, VkCommandBufferUsageFlags flags) const
    {
        RT_ASSERT(cmdBuffers[slotIdx] != VK_NULL_HANDLE, "CommandBuffer::begin on invalid buffer");

        vkResetCommandBuffer(cmdBuffers[slotIdx], 0);

        auto beginInfo  = VkCommandBufferBeginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = flags;

        CHECK_VK(vkBeginCommandBuffer(cmdBuffers[slotIdx], &beginInfo), "failed to begin command buffer!");
    }

    void CommandBuffer::end(uint32_t slotIdx) const
    {
        RT_ASSERT(cmdBuffers[slotIdx] != VK_NULL_HANDLE, "CommandBuffer::end on invalid buffer");
        CHECK_VK(vkEndCommandBuffer(cmdBuffers[slotIdx]), "failed to end command buffer!");
    }

} // namespace RT::Vulkan
