#pragma once

#if WITH_VULKAN

#include "window_sdl.h"
#include <vulkan/vulkan.h>

class sdl_window_vulkan : public sdl_window
{
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkCreateFence vkCreateFence;
    PFN_vkDestroyFence vkDestroyFence;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
    PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
    PFN_vkCreateCommandPool vkCreateCommandPool;
    PFN_vkDestroyCommandPool vkDestroyCommandPool;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
    PFN_vkQueuePresentKHR vkQueuePresentKHR;
    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
    PFN_vkEndCommandBuffer vkEndCommandBuffer;
    PFN_vkQueueSubmit vkQueueSubmit;
    PFN_vkWaitForFences vkWaitForFences;
    PFN_vkResetFences vkResetFences;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;

    VkInstance instance = nullptr;
    VkSurfaceKHR surface = nullptr;
    VkDevice vkDevice = nullptr;
    VkQueue vkQueue = nullptr;
    VkSwapchainKHR swapchain = nullptr;
    VkFence fence = nullptr;
    VkCommandPool commandPool = nullptr;
    std::vector<std::pair<VkCommandBuffer, VkFence>> commandBuffers;

    static constexpr uint64_t timeout = 60000000000ul;

    VkSurfaceFormatKHR format;
    std::vector<goopax::image_buffer<2, Eigen::Vector<uint8_t, 4>, true>> images;

    void draw_goopax(
        std::function<void(goopax::image_buffer<2, Eigen::Vector<Tuint8_t, 4>, true>& image)> func) final override;

    void create_swapchain();
    void destroy_swapchain();

public:
    sdl_window_vulkan(const char* name, Eigen::Vector<Tuint, 2> size, uint32_t flags = 0);
    ~sdl_window_vulkan();
};
#endif
