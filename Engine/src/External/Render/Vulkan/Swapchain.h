#pragma once
#include <memory>
#include <array>
#include <vector>
#include <vulkan/vulkan.h>

#include "Device.h"
#include "Fence.h"
#include "Semaphore.h"
#include "utils/Utils.h"
#include "utils/Constants.h"

namespace RT::Vulkan
{

    class Swapchain
    {
    public:
        Swapchain(const VkExtent2D windowExtent, const std::shared_ptr<Swapchain>& oldSwapchain = nullptr);
        ~Swapchain() = default;

        Swapchain(const Swapchain&) = delete;
        Swapchain(Swapchain&&) = delete;
        Swapchain& operator=(const Swapchain&) = delete;
        Swapchain&& operator=(Swapchain&&) = delete;
        
        static std::unique_ptr<Swapchain>& getSwapchainInstance() { return swapchainInstance; }

        void init();
        void shutdown();

        VkResult acquireNextImage(uint32_t& imageIndex, const uint8_t slotIdx);
        VkResult submitCommandBuffers(const VkCommandBuffer& frameBuffer, uint32_t& imageIndex, const uint8_t slotIdx, VkSemaphore extraWaitSem = VK_NULL_HANDLE, VkSemaphore extraWaitSem2 = VK_NULL_HANDLE);

        void waitSlotFence(const uint8_t slotIdx) const { inFlightFences[slotIdx].wait(); }
        bool compareFormats(const Swapchain& other) const;
        static VkFormat findDepthFormat();

        VkSwapchainKHR getSwapChain() const { return swapChain; }
        VkRenderPass getRenderPass() const { return renderPass; }
        const std::vector<VkFramebuffer>& getFramebuffers() const { return swapChainFramebuffers; }
        VkExtent2D getWindowExtent() const { return windowExtent; }
        VkExtent2D getSwapchainExtent() const { return swapChainExtent; }
        const std::vector<VkImage>& getSwapChainImages() const { return swapChainImages; }
        const uint32_t getImageCount() const { return imageCount; }
        const std::vector<VkImageView>& getSwapChainImageViews() const { return swapChainImageViews; }
        VkFormat getImageFormat() const { return swapChainImageFormat; }

        static constexpr uint32_t minImageCount() { return 2; }

    private:
        void createSwapChain();
        void createImageViews();
        void createRenderPass();
        void createFramebuffers();
        void createSyncObjects();

        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
        bool compareSwapFormats(const Swapchain& swapChain) const;
    
    private:
        VkSwapchainKHR swapChain = {};
        VkRenderPass renderPass = {};
        std::vector<VkFramebuffer> swapChainFramebuffers = {};

        VkFormat swapChainImageFormat = {};
        VkFormat swapChainDepthFormat = {};
        VkExtent2D swapChainExtent = {};
        const VkExtent2D windowExtent = {};

        std::vector<VkImage> swapChainImages = {};
        std::vector<VkImageView> swapChainImageViews = {};
        uint32_t imageCount = 0u;

        Semaphores imageAvailableSemaphores = {};
        std::vector<Semaphore> renderFinishedSemaphores = {};
        Fences inFlightFences = {};

        std::shared_ptr<Swapchain> oldSwapchain = nullptr;

        static std::unique_ptr<Swapchain> swapchainInstance;
    };

    #define SwapchainInstance ::RT::Vulkan::Swapchain::getSwapchainInstance()

}
