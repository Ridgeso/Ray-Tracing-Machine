#include <unordered_set>
#include "Debug.h"

#include "Engine/Core/Assert.h"

#include "External/Render/Vulkan/Device.h"
#include "External/Window/GlfwWindow/Utils.h"

namespace
{
	
	static VkDebugUtilsMessengerEXT GlobDebugMessenger = nullptr;
	
	static PFN_vkSetDebugUtilsObjectNameEXT pfnSetDebugUtilsObjectNameEXT = nullptr;
	static PFN_vkSetDebugUtilsObjectTagEXT pfnSetDebugUtilsObjectTagEXT = nullptr;
	static PFN_vkCmdBeginDebugUtilsLabelEXT pfnCmdBeginDebugUtilsLabelEXT = nullptr;
	static PFN_vkCmdEndDebugUtilsLabelEXT pfnCmdEndDebugUtilsLabelEXT = nullptr;

	VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		RT_LOG_ERROR("[Vulkan] ERR: {}", pCallbackData->pMessage);
		return VK_FALSE;
	}

	VkResult createDebugUtilsMessengerEXT(
		VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pDebugMessenger)
	{
		auto createValidation = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
			instance,
			"vkCreateDebugUtilsMessengerEXT");
		return createValidation ?
			createValidation(instance, pCreateInfo, pAllocator, pDebugMessenger) :
			VK_ERROR_EXTENSION_NOT_PRESENT;
	}

	void destroyDebugUtilsMessengerEXT(
		VkInstance instance,
		const VkAllocationCallbacks* pAllocator)
	{
		auto destroyValidation = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
			instance,
			"vkDestroyDebugUtilsMessengerEXT");
		if (destroyValidation != nullptr)
		{
			destroyValidation(instance, GlobDebugMessenger, pAllocator);
		}
	}

	void validateRequiredInstanceExtensions()
	{
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		auto extensions = std::vector<VkExtensionProperties>(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		auto available = std::unordered_set<std::string>{};
		for (const auto& extension : extensions)
		{
			available.insert(extension.extensionName);
		}

		auto requiredExtensions = RT::Vulkan::getRequiredExtensions();
		for (const auto& required : requiredExtensions)
		{
			RT_ASSERT(available.find(required) != available.end(), "Missing required extention: {}", required);
		}
	}

	void loadMissingDebugFunctions()
	{
		if constexpr (RT::Vulkan::EnableValidationLayers)
		{
			auto instance = DeviceInstance.getInstance();
			pfnSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT");
			pfnSetDebugUtilsObjectTagEXT  = (PFN_vkSetDebugUtilsObjectTagEXT) vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectTagEXT");
			pfnCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT");
			pfnCmdEndDebugUtilsLabelEXT   = (PFN_vkCmdEndDebugUtilsLabelEXT)  vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT");
		}
	}

}

namespace RT::Vulkan
{
	
	void setDebugObjectName(const VkObjectType objectType, const uint64_t objectHandle, const std::string& objectName)
	{
		if constexpr (EnableValidationLayers)
		{
			auto nameInfo = VkDebugUtilsObjectNameInfoEXT{};
			nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			nameInfo.objectType = objectType;
			nameInfo.objectHandle = objectHandle;
			nameInfo.pObjectName = objectName.c_str();

			pfnSetDebugUtilsObjectNameEXT(DeviceInstance.getDevice(), &nameInfo);
		}
	}
	
	void setDebugObjectTag(const VkObjectType objectType, const uint64_t objectHandle, const uint64_t tagSize, const void* tagData)
	{
		if constexpr (EnableValidationLayers)
		{
			auto tagInfo = VkDebugUtilsObjectTagInfoEXT{};
			tagInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_TAG_INFO_EXT,
			tagInfo.objectType = objectType;
			tagInfo.objectHandle = objectHandle;
			tagInfo.tagSize = tagSize;
			tagInfo.tagName = 0;
			tagInfo.pTag = tagData;

			pfnSetDebugUtilsObjectTagEXT(DeviceInstance.getDevice(), &tagInfo);
		}
	}

	void beginDebugLabel(VkCommandBuffer cmdBuff, const std::string& labelName, const glm::vec4 colour)
	{
		if constexpr (EnableValidationLayers)
		{
			auto labelInfo = VkDebugUtilsLabelEXT{};
			labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
			labelInfo.pLabelName = labelName.c_str();
			std::memcpy(labelInfo.color, &colour, sizeof(float[4]));
			pfnCmdBeginDebugUtilsLabelEXT(cmdBuff, &labelInfo);
		}
	}

	void endDebugLabel(VkCommandBuffer cmdBuff)
	{
		if constexpr (EnableValidationLayers)
		{
			pfnCmdEndDebugUtilsLabelEXT(cmdBuff);
		}
	}

	bool checkValidationLayerSupport()
	{
		if constexpr (not EnableValidationLayers)
		{
			return true;
		}

		uint32_t layerCount = 0u;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		auto availableLayers = std::vector<VkLayerProperties>(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (auto layerName : ValidationLayers)
		{
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers)
			{
				if (strcmp(layerName, layerProperties.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound)
			{
				return false;
			}
		}

		return true;
	}

	void checkVkResultCallback(const VkResult result)
	{
		RT_ASSERT(VK_SUCCESS == result, "[Vulkan] Required immediate abort VkResult = {}", result);
	}

	std::vector<const char*> getRequiredExtensions()
	{
		auto extensions = Glfw::getInstanceExtensions();

		if constexpr (EnableValidationLayers)
		{
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			VK_EXT_debug_utils;
		}

		return extensions;
	}

	VkDebugUtilsMessengerCreateInfoEXT populateDebugMessengerCreateInfo()
	{
		auto createInfo = VkDebugUtilsMessengerCreateInfoEXT{};
		if constexpr (EnableValidationLayers)
		{
			createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			createInfo.pfnUserCallback = debugCallback;
			createInfo.pUserData = nullptr;
		}
		return createInfo;
	}

	void setupDebugMessenger(VkInstance instance)
	{
		validateRequiredInstanceExtensions();
		if constexpr (EnableValidationLayers)
		{
			loadMissingDebugFunctions();

			auto createInfo = populateDebugMessengerCreateInfo();
			RT_ASSERT(createDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &GlobDebugMessenger) == VK_SUCCESS, "failed to set up debug messenger!");
		}
	}

	void closeDebugMessenger(VkInstance instance)
	{
		if constexpr (EnableValidationLayers)
		{
			destroyDebugUtilsMessengerEXT(instance, nullptr);
		}
	}

}
