#pragma once
#include <string>
#include <vector>
#include <array>
#include <glm/glm.hpp>

#include "Engine/Core/Assert.h"

#include <vulkan/vulkan.h>

namespace RT::Vulkan
{

#ifdef VK_DEBUG
    constexpr bool EnableValidationLayers = true;
#else
    constexpr bool EnableValidationLayers = false;
#endif // VK_DEBUG
    constexpr std::array<const char*, 1> ValidationLayers = { "VK_LAYER_KHRONOS_validation" };

    void setDebugObjectName(const VkObjectType objectType, const uint64_t objectHandle, const std::string& objectName);
    void setDebugObjectTag(const VkObjectType objectType, const uint64_t objectHandle, const uint64_t tagSize, const void* tagData);
    void beginDebugLabel(VkCommandBuffer cmdBuff, const std::string& labelName, const glm::vec4 colour);
    void endDebugLabel(VkCommandBuffer cmdBuff);

    bool checkValidationLayerSupport();
    void checkVkResultCallback(const VkResult result);

    std::vector<const char*> getRequiredExtensions();

    template <typename CreateInfo>
    void enableDebugingForCreateInfo(CreateInfo& createInfo, VkDebugUtilsMessengerCreateInfoEXT* debugCreateInfo = nullptr)
    {
        if constexpr (EnableValidationLayers)
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
    VkDebugUtilsMessengerCreateInfoEXT populateDebugMessengerCreateInfo();

    void setupDebugMessenger(VkInstance instance);
    void closeDebugMessenger(VkInstance instance);

}

#ifdef VK_DEBUG
    #define SET_VK_DEBUG_NAME(OBJECT_TYPE, OBJECT_HANDLE, OBJECT_NAME)       RT::Vulkan::setDebugObjectName(OBJECT_TYPE, OBJECT_HANDLE, OBJECT_NAME)
    #define SET_VK_DEBUG_TAG(OBJECT_TYPE, OBJECT_HANDLE, TAG_SIZE, TAG_DATA) RT::Vulkan::setDebugObjectTag(OBJECT_TYPE, OBJECT_HANDLE, TAG_SIZE, TAG_DATA)
    #define BEGIN_VK_DEBUG_LABEL(CMD_BUFF, LABEL_NAME, COLOR)                RT::Vulkan::beginDebugLabel(CMD_BUFF, LABEL_NAME, COLOR)
    #define END_VK_DEBUG_LABEL(CMD_BUFF)                                     RT::Vulkan::endDebugLabel(CMD_BUFF)

    #define CHECK_VK(EXPR, MSG) { VkResult result = EXPR; if (VK_SUCCESS != result) { RT_LOG_CRITICAL("Result id = {}: " MSG, (uint32_t)result); DEBUGBREAK; } }
#else
    #define SET_VK_DEBUG_NAME()
    #define SET_VK_DEBUG_TAG()
    #define BEGIN_VK_DEBUG_LABEL()
    #define END_VK_DEBUG_LABEL()

    #define CHECK_VK(EXPR, MSG, ...) EXPR
#endif // VK_DEBUG
