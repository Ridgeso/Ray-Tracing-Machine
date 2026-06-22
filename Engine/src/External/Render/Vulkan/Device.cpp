#include "Device.h"
#include <unordered_set>

#include "utils/Debug.h"

#include "Engine/Core/Application.h"

#include <GLFW/glfw3.h>

namespace RT::Vulkan
{
namespace
{
    static constexpr std::array<const char*, 1> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    struct DeviceFeatrues
    {
        VkPhysicalDeviceFeatures deviceFeatures = {};
        VkPhysicalDeviceVulkan12Features vulkan12Features = {};
        VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexingFeatures = {};
    };

    constexpr DeviceFeatrues deviceVulkanFeatures()
    {
        auto features = DeviceFeatrues{};

        /*
        * VkPhysicalDeviceFeatures
        */
        features.deviceFeatures.samplerAnisotropy = VK_TRUE;
        features.deviceFeatures.shaderFloat64 = VK_TRUE;

        /*
        * VkPhysicalDeviceVulkan12Features
        */
        features.vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        features.vulkan12Features.runtimeDescriptorArray = VK_TRUE;
        
        /*
        * VkPhysicalDeviceDescriptorIndexingFeaturesEXT
        */
        features.indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
        features.indexingFeatures.runtimeDescriptorArray = VK_TRUE;

        return features;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice phyDev)
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

    std::vector<VkQueueFamilyProperties> getQueueFamilyProperties(VkPhysicalDevice phyDev)
    {
        uint32_t queueFamilyCount = 0u;
        vkGetPhysicalDeviceQueueFamilyProperties(phyDev, &queueFamilyCount, nullptr);
        auto queueFamilies = std::vector<VkQueueFamilyProperties>(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(phyDev, &queueFamilyCount, queueFamilies.data());
        return queueFamilies;
    }
} // namespace
    
    Device Device::deviceInstance = Device{};

    void Device::init()
    {
        LOG_DEBUG("VULKAN", "Device Instantiation");
        createInstance();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createCommandPool();
    }

    void Device::shutdown()
    {
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyCommandPool(device, computeCommandPool, nullptr);

        vkDestroyDevice(device, nullptr);
         
        closeDebugMessenger(instance);
       
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
    }

    void Device::waitForIdle() const
    {
        auto lock = std::lock_guard<std::mutex>{ queueMutex };
        vkDeviceWaitIdle(device);
    }

    VkResult Device::queueSubmit(VkQueue queue, const uint32_t submitCount, const VkSubmitInfo* submits, const VkFence fence) const
    {
        auto lock = std::lock_guard<std::mutex>{ queueMutex };
        return vkQueueSubmit(queue, submitCount, submits, fence);
    }

    VkResult Device::queuePresent(VkQueue queue, const VkPresentInfoKHR* presentInfo) const
    {
        auto lock = std::lock_guard<std::mutex>{ queueMutex };
        return vkQueuePresentKHR(queue, presentInfo);
    }

    void Device::queueWaitIdle(VkQueue queue) const
    {
        auto lock = std::lock_guard<std::mutex>{ queueMutex };
        vkQueueWaitIdle(queue);
    }

    void Device::createImageWithInfo(
        const VkImageCreateInfo& imageInfo,
        const VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& imageMemory) const
    {
        CHECK_VK(vkCreateImage(device, &imageInfo, nullptr, &image), "failed to create image!");

        auto memRequirements = VkMemoryRequirements{};
        vkGetImageMemoryRequirements(device, image, &memRequirements);

        auto allocInfo = VkMemoryAllocateInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        CHECK_VK(vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory), "failed to allocate image memory!");
        CHECK_VK(vkBindImageMemory(device, image, imageMemory, 0), "failed to bind image memory!");
    }

    CommandBuffer Device::createCommandBuffer(const VkCommandPool commandPool, const VkCommandBufferLevel level) const
    {
        auto commandBuffer = CommandBuffer{};
        commandBuffer.create(commandPool, level);
        return commandBuffer;
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
        ASSERT("VULKAN", false, "failed to find supported format!");
        return VK_FORMAT_UNDEFINED;
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

        CHECK_VK(
            vkCreateBuffer(device, &bufferInfo, nullptr, &buffer),
            "failed to create vertex buffer");

        auto memRequirements = VkMemoryRequirements{};
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        auto allocInfo = VkMemoryAllocateInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        CHECK_VK(
            vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory),
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
        ASSERT("VULKAN", false, "failed to find suitable memory type!");
        return 0xFFFFFFFF;
    }

    void Device::createInstance()
    {
        constexpr uint32_t apiVersion = VK_HEADER_VERSION_COMPLETE;

        auto appInfo = VkApplicationInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "RT App";
        appInfo.applicationVersion = apiVersion;
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = apiVersion;
        appInfo.apiVersion = apiVersion;

        auto createInfo = VkInstanceCreateInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        auto debugCreateInfo = populateDebugMessengerCreateInfo();
        enableDebugingForCreateInfo(createInfo, &debugCreateInfo);

        CHECK_VK(vkCreateInstance(&createInfo, nullptr, &instance), "failed to create Vulkan Instance");
        LOG_DEBUG("VULKAN", "Instance created: {{ apiVersion = {}.{}.{}.{} }}",
            apiVersion >> 29,
            (apiVersion >> 22) & 0b1111111,
            (apiVersion >> 12) & 0b1111111111,
            apiVersion & 0b111111111111);

        setupDebugMessenger(instance);
    }

    void Device::createSurface()
    {
        CHECK_VK(
            glfwCreateWindowSurface(
                instance,
                (GLFWwindow*)Application::getWindow()->getNativWindow(),
                nullptr,
                &surface),
            "failed to craete window surface");
    }

    void Device::pickPhysicalDevice()
    {
        uint32_t deviceCount = 0u;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        
        ASSERT("VULKAN", deviceCount != 0, "Failed to find any GPU supporting Vulkan");

        auto devices = std::vector<VkPhysicalDevice>(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        auto phiDev = std::find_if(devices.begin(), devices.end(), [this](auto dev) { return isDeviceSuitable(dev); });
        ASSERT("VULKAN", phiDev != devices.end(), "Failed to find any suitable GPU");
        physicalDevice = *phiDev;

        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
        LOG_DEBUG("VULKAN", "Found device: {}", deviceProperties.deviceName);
    }

    void Device::createLogicalDevice()
    {
        swapChainSupportDetails = querySwapChainSupport(physicalDevice);
        queueFamilyIndices = findQueueFamilies(physicalDevice);

        auto queueCreateInfos = std::vector<VkDeviceQueueCreateInfo>{};
        auto uniqueQueueFamilies = std::vector<Utils::QueueFamily>{ *queueFamilyIndices.graphics, *queueFamilyIndices.compute , *queueFamilyIndices.present };
        
        auto queuesPriorities = std::unordered_map<uint32_t, std::vector<float>>{};
        for (const auto& queueFamily : uniqueQueueFamilies)
        {
            auto it = std::find_if(
                queueCreateInfos.begin(),
                queueCreateInfos.end(),
                [&queueFamily](const auto& info) { return info.queueFamilyIndex == queueFamily.index; });
            if (it == queueCreateInfos.end())
            {
                it = queueCreateInfos.insert(queueCreateInfos.end(), VkDeviceQueueCreateInfo{});
            }

            it->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            it->queueFamilyIndex = queueFamily.index;
            it->queueCount = std::max(it->queueCount, queueFamily.offset + queueFamily.count);

            auto& queuePriority = queuesPriorities[queueFamily.index];
            queuePriority.resize(it->queueCount);
            std::fill_n(queuePriority.begin() + queueFamily.offset, queueFamily.count, queueFamily.priority);
            it->pQueuePriorities = queuePriority.data();
        }

        auto vulkanFeatures = deviceVulkanFeatures();

        auto createInfo = VkDeviceCreateInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &vulkanFeatures.deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        enableDebugingForCreateInfo(createInfo);

        if (deviceProperties.apiVersion >= VK_API_VERSION_1_2)
        {
            createInfo.pNext = &vulkanFeatures.vulkan12Features;
        }
        else
        {
            createInfo.pNext = &vulkanFeatures.indexingFeatures;
        }

        CHECK_VK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device), "failed to create logical device");

        setupQueues();
        
        LOG_DEBUG(
            "VULKAN",
            "Queue families: {{ graphics = {}, present = {}, compute = {}{} }}",
            queueFamilyIndices.graphics->index,
            queueFamilyIndices.present->index,
            queueFamilyIndices.compute->index,
            queueFamilyIndices.dedicatedComputeFamily ? " (dedicated async)" : "");
    }

    void Device::createCommandPool()
    {
        auto poolInfo = VkCommandPoolCreateInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphics->index;
        CHECK_VK(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool), "failed to create command pool!");

        poolInfo.queueFamilyIndex = queueFamilyIndices.compute->index;
        CHECK_VK(vkCreateCommandPool(device, &poolInfo, nullptr, &computeCommandPool), "failed to create compute command pool!");
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

        return
            indices.graphics and
            indices.compute and
            indices.present and
            extensionsSupported and
            swapChainAdequate and
            supportedFeatures.samplerAnisotropy;
    }

    Utils::QueueFamilyIndices Device::findQueueFamilies(VkPhysicalDevice phyDev) const
    {
        auto indices = Utils::QueueFamilyIndices{};

        auto queueFamilies = getQueueFamilyProperties(phyDev);

        constexpr float weight = 1.0f;
        for (uint32_t i = 0; i < queueFamilies.size(); ++i)
        {
            const auto& queueFamily = queueFamilies[i];

            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.graphics = { i, 0, queueFamily.queueCount, weight };
            }

            auto presentSupport = VkBool32(false);
            vkGetPhysicalDeviceSurfaceSupportKHR(phyDev, i, surface, &presentSupport);
            if (presentSupport)
            {
                indices.present = { i, 0, queueFamily.queueCount, weight };
            }

            if (indices.graphics and indices.present)
            {
                break;
            }
        }

        for (uint32_t i = 0; i < queueFamilies.size(); ++i)
        {
            const auto& queueFamily = queueFamilies[i];

            if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                indices.compute = { i, 0, queueFamily.queueCount, weight };
            }

            if (indices.compute and indices.graphics and indices.compute->index != indices.graphics->index)
            {
                indices.dedicatedComputeFamily = true;
                break;
            }
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
        CHECK_VK(vkAllocateCommandBuffers(device, &allocInfo, &cmdBuffer), "failed to allocate command buffers!");

        auto beginInfo = VkCommandBufferBeginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        CHECK_VK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "failed to begin command buffer");

        return cmdBuffer;
    }

    void Device::flushSingleCmdBuff(const VkCommandBuffer cmdBuffer) const
    {
        CHECK_VK(vkEndCommandBuffer(cmdBuffer), "failed to end command buffer");

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;

        queueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        queueWaitIdle(graphicsQueue);

        vkFreeCommandBuffers(device, commandPool, 1, &cmdBuffer);
    }

    void Device::setupQueues()
    {
        auto queueFamilies = getQueueFamilyProperties(physicalDevice);
        queueOccupancy.resize(queueFamilies.size());
        for (uint32_t i = 0; i < queueFamilies.size(); ++i)
        {
            queueOccupancy[i] = std::vector<uint32_t>(queueFamilies[i].queueCount, 0u);
        }

        const auto& grap = *queueFamilyIndices.graphics;
        const auto& pres = *queueFamilyIndices.present;
        const auto& comp = *queueFamilyIndices.compute;

        queueOccupancyMapping[grap.index] = std::span(queueOccupancy[grap.index].begin() + grap.offset, grap.count);
        queueOccupancyMapping[pres.index] = std::span(queueOccupancy[pres.index].begin() + pres.offset, pres.count);
        queueOccupancyMapping[comp.index] = std::span(queueOccupancy[comp.index].begin() + comp.offset, comp.count);

        vkGetDeviceQueue(device, grap.index, getQueueIdx(grap.index), &graphicsQueue);
        vkGetDeviceQueue(device, grap.index, getQueueIdx(grap.index), &imGuiQueue);
        vkGetDeviceQueue(device, pres.index, getQueueIdx(pres.index), &presentQueue);
        vkGetDeviceQueue(device, comp.index, getQueueIdx(comp.index), &computeQueue);
    }

    uint32_t Device::getQueueIdx(const uint32_t queueFamilyIndex)
    {
        auto& occupancy = queueOccupancyMapping[queueFamilyIndex];
        auto smallestIt = occupancy.begin();
        for (auto nIt = smallestIt + 1; nIt != occupancy.end(); ++nIt)
        {
            if (*nIt < *smallestIt)
            {
                smallestIt = nIt;
            }
        }
        *smallestIt += 1;
        return std::distance(occupancy.begin(), smallestIt);
    }

} // namespac RT::Vulkan
