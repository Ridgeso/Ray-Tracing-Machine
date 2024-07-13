#pragma once
#include <vector>
#include <array>

#include <vulkan/vulkan.h>

namespace RT::Vulkan
{

    constexpr bool EnableValidationLayers = true;
    constexpr std::array<const char*, 1> ValidationLayers = { "VK_LAYER_KHRONOS_validation" };

    bool checkValidationLayerSupport();

    std::vector<const char*> getRequiredExtensions();

    VkResult createDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pDebugMessenger);

    void destroyDebugUtilsMessengerEXT(VkInstance instance, const VkAllocationCallbacks* pAllocator);
    
    void checkVkResultCallback(VkResult result);

    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    VkDebugUtilsMessengerCreateInfoEXT populateDebugMessengerCreateInfo();

    template <typename CreateInfo>
    void enableDebugingForCreateInfo(CreateInfo& createInfo, VkDebugUtilsMessengerCreateInfoEXT* debugCreateInfo = nullptr)
    {
        if (EnableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
            createInfo.ppEnabledLayerNames = ValidationLayers.data();
            createInfo.pNext = debugCreateInfo;
        }
        else
        {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }
    }

    void setupDebugMessenger(VkInstance& instance);

}
