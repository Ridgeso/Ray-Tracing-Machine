#pragma once
#include <cstdint>
#include <optional>
#include <vector>

#include <vulkan/vulkan.h>

namespace RT::Vulkan::Utils
{

    struct QueueFamily
    {
        uint32_t index;
        uint32_t count;
        float priority;
    };

    struct QueueFamilyIndices
    {
        std::optional<QueueFamily> graphics;
        std::optional<QueueFamily> present;
        std::optional<QueueFamily> compute;
        bool dedicatedComputeFamily = false;
    };

    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

}
