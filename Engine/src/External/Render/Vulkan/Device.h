#pragma once
#include <array>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <span>

#include <vulkan/vulkan.h>

#include "CommandBuffer.h"
#include "utils/Utils.h"

namespace RT::Vulkan
{
    
    class Device
    {
    public:
        ~Device() = default;
        
        Device(const Device&) = delete;
        Device(Device&&) = delete;
        Device& operator=(const Device&) = delete;
        Device&& operator=(Device&&) = delete;

        static Device& getDeviceInstance() { return deviceInstance; }

        void init();
        void shutdown();

        void waitForIdle() const;

        VkResult queueSubmit(VkQueue queue, const uint32_t submitCount, const VkSubmitInfo* submits, const VkFence fence) const;
        VkResult queuePresent(VkQueue queue, const VkPresentInfoKHR* presentInfo) const;
        void queueWaitIdle(VkQueue queue) const;

        void createImageWithInfo(
            const VkImageCreateInfo& imageInfo,
            const VkMemoryPropertyFlags properties,
            VkImage& image,
            VkDeviceMemory& imageMemory) const;
        VkFormat findSupportedFormat(
            const std::vector<VkFormat>& candidates,
            const VkImageTiling tiling,
            VkFormatFeatureFlags features) const;
        void createBuffer(
            const VkDeviceSize size,
            const VkBufferUsageFlags usage,
            const VkMemoryPropertyFlags properties,
            VkBuffer& buffer,
            VkDeviceMemory& bufferMemory) const;
        CommandBuffer createCommandBuffer(VkCommandPool commandPool, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const;
        uint32_t findMemoryType(const uint32_t typeFilter, const VkMemoryPropertyFlags properties) const;

        template <typename Proc>
        void execSingleCmdPass(Proc&& proc) const
        {
            auto singleCmdBuff = startSingleCmdBuff();
            proc(singleCmdBuff);
            flushSingleCmdBuff(singleCmdBuff);
        }

        VkDevice getDevice() const { return device; }
        VkInstance getInstance() const { return instance; }
        VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
        VkSurfaceKHR getSurface() const { return surface; }
        VkQueue getGraphicsQueue() const { return graphicsQueue; }
        VkQueue getImGuiQueue() const { return imGuiQueue; }
        VkQueue getPresentQueue() const { return presentQueue; }
        VkQueue getComputeQueue() const { return computeQueue; }
        VkCommandPool getCommandPool() const { return commandPool; }
        VkCommandPool getComputeCommandPool() const { return computeCommandPool; }

        const VkPhysicalDeviceLimits& getLimits() const { return deviceProperties.limits; }
        
        const Utils::SwapChainSupportDetails& getSwapChainSupportDetails() const { return swapChainSupportDetails; }
        Utils::QueueFamilyIndices getQueueFamilyIndices() const { return queueFamilyIndices; }

        friend class Swapchain;
    private:
        Device() = default;

        void createInstance();
        void createSurface();
        void pickPhysicalDevice();
        void createLogicalDevice();
        void createCommandPool();

        bool isDeviceSuitable(VkPhysicalDevice phyDev);
        Utils::QueueFamilyIndices findQueueFamilies(VkPhysicalDevice phyDev) const;
        Utils::SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice phyDev);

        VkCommandBuffer startSingleCmdBuff() const;
        void flushSingleCmdBuff(const VkCommandBuffer commandBuffer) const;

        void setupQueues();
        uint32_t getQueueIdx(const uint32_t queueFamilyIndex);

    private:
        VkDevice device = {};
        VkInstance instance = {};
        VkSurfaceKHR surface = {};
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

        VkQueue graphicsQueue = {};
        VkQueue imGuiQueue = {};
        VkQueue presentQueue = {};
        VkQueue computeQueue = {};

        VkCommandPool commandPool = {};
        VkCommandPool computeCommandPool = {};
        
        VkPhysicalDeviceProperties deviceProperties = {};

        Utils::SwapChainSupportDetails swapChainSupportDetails = {};
        Utils::QueueFamilyIndices queueFamilyIndices = {};

        mutable std::mutex queueMutex = {};

        std::vector<std::vector<uint32_t>> queueOccupancy = {};
        std::unordered_map<uint32_t, std::span<uint32_t>> queueOccupancyMapping = {};

        static Device deviceInstance;
    };

    #define DeviceInstance ::RT::Vulkan::Device::getDeviceInstance()

}
