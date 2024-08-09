#include "Device.h"
#include <unordered_set>

#include "Engine/Core/Log.h"
#include "Engine/Core/Assert.h"
#include "utils/Debug.h"

#include "Engine/Core/Application.h"

#include <GLFW/glfw3.h>

namespace RT::Vulkan
{
    
    Device Device::deviceInstance = Device{};

    void Device::init()
    {
        createInstance();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createCommandPool();
    }

    void Device::shutdown()
    {
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyDevice(device, nullptr);
         
        destroyDebugUtilsMessengerEXT(instance, nullptr);
       
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
    }

    void Device::createImageWithInfo(
        const VkImageCreateInfo& imageInfo,
        const VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& imageMemory) const
    {
        RT_CORE_ASSERT(vkCreateImage(device, &imageInfo, nullptr, &image) == VK_SUCCESS, "failed to create image!");

        auto memRequirements = VkMemoryRequirements{};
        vkGetImageMemoryRequirements(device, image, &memRequirements);

        auto allocInfo = VkMemoryAllocateInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        RT_CORE_ASSERT(vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) == VK_SUCCESS, "failed to allocate image memory!");
        RT_CORE_ASSERT(vkBindImageMemory(device, image, imageMemory, 0) == VK_SUCCESS, "failed to bind image memory!");
    }

    VkFormat Device::findSupportedFormat(
        const std::vector<VkFormat>& candidates,
        const VkImageTiling tiling,
        VkFormatFeatureFlags features) const
    {
        for (auto format : candidates)
        {
            auto props = VkFormatProperties{};
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

            if ((tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
                || (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features))
            {
                return format;
            }
        }
        RT_CORE_ASSERT(false, "failed to find supported format!");
    }

    void Device::createBuffer(
        const VkDeviceSize size,
        const VkBufferUsageFlags usage,
        const VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory) const
    {
        auto bufferInfo = VkBufferCreateInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        RT_CORE_ASSERT(
            vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) == VK_SUCCESS,
            "failed to create vertex buffer");

        auto memRequirements = VkMemoryRequirements{};
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        auto allocInfo = VkMemoryAllocateInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        RT_CORE_ASSERT(
            vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) == VK_SUCCESS,
            "failed to allocate vertex buffer");

        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    }

    uint32_t Device::findMemoryType(const uint32_t typeFilter, const VkMemoryPropertyFlags properties) const
    {
        auto memProperties = VkPhysicalDeviceMemoryProperties{};
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
        for (uint32_t memType = 0u; memType < memProperties.memoryTypeCount; memType++)
        {
            if ((typeFilter & (1 << memType)) && (memProperties.memoryTypes[memType].propertyFlags & properties) == properties)
            {
                return memType;
            }
        }
        RT_CORE_ASSERT(false, "failed to find suitable memory type!");
    }

    void Device::createInstance()
    {
        auto appInfo = VkApplicationInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "RT App";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        auto createInfo = VkInstanceCreateInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        auto debugCreateInfo = populateDebugMessengerCreateInfo();
        enableDebugingForCreateInfo(createInfo, &debugCreateInfo);

        RT_ASSERT(vkCreateInstance(&createInfo, nullptr, &instance) == VK_SUCCESS);

        validateRequiredInstanceExtensions();
        setupDebugMessenger(instance);
    }

    void Device::createSurface()
    {
        RT_CORE_ASSERT(
            glfwCreateWindowSurface(
                instance,
                (GLFWwindow*)Application::getWindow()->getNativWindow(),
                nullptr,
                &surface) == VK_SUCCESS,
            "failed to craete window surface");
    }

    void Device::pickPhysicalDevice()
    {
        uint32_t deviceCount = 0u;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        
        RT_ASSERT(deviceCount != 0, "Failed to find any GPU supporting Vulkan");

        auto devices = std::vector<VkPhysicalDevice>(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        auto phiDev = std::find_if(devices.begin(), devices.end(), [this](auto dev) { return isDeviceSuitable(dev); });
        RT_CORE_ASSERT(phiDev != devices.end(), "Failed to find any suitable GPU");
        physicalDevice = *phiDev;

        if (EnableValidationLayers)
        {
            vkGetPhysicalDeviceProperties(physicalDevice, &properties);
            RT_LOG_INFO(
                "physical device: {}", properties.deviceName);
        }
    }

    void Device::createLogicalDevice()
    {
        swapChainSupportDetails = querySwapChainSupport(physicalDevice);
        queueFamilyIndices = findQueueFamilies(physicalDevice);

        auto queueCreateInfos = std::vector<VkDeviceQueueCreateInfo>{};
        auto uniqueQueueFamilies = std::unordered_set<uint32_t>{ queueFamilyIndices.graphicsFamily, queueFamilyIndices.presentFamily };

        float queuePriority = 1.0f;
        for (auto queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo = {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        auto deviceFeatures = VkPhysicalDeviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;

        auto createInfo = VkDeviceCreateInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        enableDebugingForCreateInfo(createInfo);
        
        RT_CORE_ASSERT(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) == VK_SUCCESS, "failed to create logical device");

        vkGetDeviceQueue(device, queueFamilyIndices.graphicsFamily, 0, &graphicsQueue);
        vkGetDeviceQueue(device, queueFamilyIndices.presentFamily, 0, &presentQueue);
    }

    void Device::createCommandPool()
    {
        auto poolInfo = VkCommandPoolCreateInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
        poolInfo.flags =
            VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        RT_CORE_ASSERT(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) == VK_SUCCESS, "failed to create command pool!");
    }

    void Device::validateRequiredInstanceExtensions() const
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

        auto requiredExtensions = getRequiredExtensions();
        for (const auto& required : requiredExtensions)
        {
            RT_CORE_ASSERT(available.find(required) != available.end(), "Missing required extention: {}", required);
        }
    }

    bool Device::isDeviceSuitable(VkPhysicalDevice phyDev)
    {
        bool swapChainAdequate = false;
        auto indices = findQueueFamilies(phyDev);
        auto extensionsSupported = checkDeviceExtensionSupport(phyDev);
        
        if (extensionsSupported)
        {
            auto swapChainSupport = querySwapChainSupport(phyDev);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        auto supportedFeatures = VkPhysicalDeviceFeatures{};
        vkGetPhysicalDeviceFeatures(phyDev, &supportedFeatures);

        return indices.graphicsFamilyHasValue &&
            indices.presentFamilyHasValue &&
            extensionsSupported &&
            swapChainAdequate &&
            supportedFeatures.samplerAnisotropy;
    }

    Utils::QueueFamilyIndices Device::findQueueFamilies(VkPhysicalDevice phyDev) const
    {
        auto indices = Utils::QueueFamilyIndices{};

        uint32_t queueFamilyCount = 0u;
        vkGetPhysicalDeviceQueueFamilyProperties(phyDev, &queueFamilyCount, nullptr);

        auto queueFamilies = std::vector<VkQueueFamilyProperties>(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(phyDev, &queueFamilyCount, queueFamilies.data());

        int32_t nrOfGraphicsFamily = 0;
        for (const auto& queueFamily : queueFamilies)
        {
            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.graphicsFamily = nrOfGraphicsFamily;
                indices.graphicsFamilyHasValue = true;
            }
            auto presentSupport = VkBool32(false);
            vkGetPhysicalDeviceSurfaceSupportKHR(phyDev, nrOfGraphicsFamily, surface, &presentSupport);
            if (queueFamily.queueCount > 0 && presentSupport)
            {
                indices.presentFamily = nrOfGraphicsFamily;
                indices.presentFamilyHasValue = true;
            }
            if (indices.graphicsFamilyHasValue && indices.presentFamilyHasValue)
            {
                break;
            }

            nrOfGraphicsFamily++;
        }

        return indices;
    }

    Utils::SwapChainSupportDetails Device::querySwapChainSupport(VkPhysicalDevice phyDev)
    {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phyDev, surface, &swapChainSupportDetails.capabilities);

        uint32_t formatCount = 0u;
        vkGetPhysicalDeviceSurfaceFormatsKHR(phyDev, surface, &formatCount, nullptr);

        if (formatCount != 0)
        {
            swapChainSupportDetails.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(phyDev, surface, &formatCount, swapChainSupportDetails.formats.data());
        }

        uint32_t presentModeCount = 0u;
        vkGetPhysicalDeviceSurfacePresentModesKHR(phyDev, surface, &presentModeCount, nullptr);

        if (presentModeCount != 0)
        {
            swapChainSupportDetails.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                phyDev,
                surface,
                &presentModeCount,
                swapChainSupportDetails.presentModes.data());
        }
        return swapChainSupportDetails;
    }

    VkCommandBuffer Device::startSingleCmdBuff() const
    {
        auto allocInfo = VkCommandBufferAllocateInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        auto cmdBuffer = VkCommandBuffer{};
        RT_CORE_ASSERT(
            vkAllocateCommandBuffers(device, &allocInfo, &cmdBuffer) == VK_SUCCESS,
            "failed to allocate command buffers!");

        auto beginInfo = VkCommandBufferBeginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        RT_CORE_ASSERT(
            vkBeginCommandBuffer(cmdBuffer, &beginInfo) == VK_SUCCESS,
            "failed to begin command buffer");

        return cmdBuffer;
    }

    void Device::flushSingleCmdBuff(const VkCommandBuffer cmdBuffer) const
    {
        RT_CORE_ASSERT(
            vkEndCommandBuffer(cmdBuffer) == VK_SUCCESS,
            "failed to end command buffer");

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;

        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);

        vkFreeCommandBuffers(device, commandPool, 1, &cmdBuffer);
    }

    bool Device::checkDeviceExtensionSupport(VkPhysicalDevice phyDev)
    {
        uint32_t extensionCount = 0u;
        vkEnumerateDeviceExtensionProperties(phyDev, nullptr, &extensionCount, nullptr);
        auto availableExtensions = std::vector<VkExtensionProperties>(extensionCount);
        vkEnumerateDeviceExtensionProperties(
            phyDev,
            nullptr,
            &extensionCount,
            availableExtensions.data());

        auto requiredExtensions = std::unordered_set<std::string>(deviceExtensions.begin(), deviceExtensions.end());
        for (const auto& extension : availableExtensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }
        return requiredExtensions.empty();
    }

}
