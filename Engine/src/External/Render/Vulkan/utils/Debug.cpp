#include "Debug.h"
#include <Engine/Core/Log.h>
#include <Engine/Core/Assert.h>

#include "External/window/GlfwWindow/Utils.h"

namespace RT::Vulkan
{

	VkDebugUtilsMessengerEXT GlobDebugMessenger{};
	
	bool checkValidationLayerSupport()
	{
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

	std::vector<const char*> getRequiredExtensions()
	{
		auto extensions = Glfw::getInstanceExtensions();

		if (EnableValidationLayers)
		{
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
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
		if (EnableValidationLayers)
		{
			auto destroyValidation = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
				instance,
				"vkDestroyDebugUtilsMessengerEXT");
			if (destroyValidation != nullptr)
			{
				destroyValidation(instance, GlobDebugMessenger, pAllocator);
			}
		}
	}

	void checkVkResultCallback(VkResult result)
	{
		//if (result > VK_SUCCESS)
		//{
		//	RT_LOG_ERROR("[Vulkan] VkResult = {}", result);
		//}
		//RT_CORE_ASSERT(result >= VK_SUCCESS, "[Vulkan] Required immediate abort VkResult = {}", result);
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		RT_LOG_ERROR("[Vulkan] ERR: {}", pCallbackData->pMessage);
		return VK_FALSE;
	}

	VkDebugUtilsMessengerCreateInfoEXT populateDebugMessengerCreateInfo()
	{
		auto createInfo = VkDebugUtilsMessengerCreateInfoEXT{};
		if (EnableValidationLayers)
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

	void setupDebugMessenger(VkInstance& instance)
	{
		if (EnableValidationLayers)
		{
			auto createInfo = populateDebugMessengerCreateInfo();
			RT_CORE_ASSERT(createDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &GlobDebugMessenger) == VK_SUCCESS, "failed to set up debug messenger!");
		}
	}

}
