#include "window_vulkan.h"
#include <SDL3/SDL_vulkan.h>
#if __has_include(<vulkan/vk_enum_string_helper.h>)
#include <vulkan/vk_enum_string_helper.h>
#endif

using namespace std;
using namespace goopax;

inline void call_vulkan(VkResult result)
{
    if (result != VK_SUCCESS) [[unlikely]]
    {
        cout << "vulkan error: " << result;
#if __has_include(<vulkan/vk_enum_string_helper.h>)
        cout << " (" << string_VkResult(result) << ")";
#endif
        cout << endl;
        throw std::runtime_error("Got vulkan error");
    }
}

void sdl_window_vulkan::draw_goopax(std::function<void(image_buffer<2, Eigen::Vector<uint8_t, 4>, true>& image)> func)
{
tryagain:
    uint32_t imageIndex;
    auto err = vkAcquireNextImageKHR(vkDevice, swapchain, timeout, nullptr, fence, &imageIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        cout << "vkAcquireNextImageKHR returned VK_OUT_OF_DATE_KHR" << endl
             << "Probably the window has been resized." << endl;
        destroy_swapchain();
        create_swapchain();
        cout << "Trying again." << endl;
        goto tryagain;
    }
    else if (err == VK_SUBOPTIMAL_KHR)
    {
        cout << "vkAcquireNextImageKHR returned VK_SUBOPTIMAL_KHR" << endl;
    }
    else
    {
        call_vulkan(err);
    }

    auto& cb = commandBuffers[imageIndex];

    call_vulkan(vkWaitForFences(vkDevice, 1, &cb.second, false, timeout));
    call_vulkan(vkResetFences(vkDevice, 1, &cb.second));

    {
        VkCommandBufferBeginInfo info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                          .pNext = nullptr,
                                          .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                          .pInheritanceInfo = nullptr };
        vkBeginCommandBuffer(cb.first, &info);
    }

    call_vulkan(vkWaitForFences(vkDevice, 1, &fence, false, timeout));

    {
        VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = 0,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = static_cast<VkImage>(images[imageIndex].get_handler()),
            .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                  .baseMipLevel = 0,
                                  .levelCount = 1,
                                  .baseArrayLayer = 0,
                                  .layerCount = 1 },
        };

        vkCmdPipelineBarrier(cb.first,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &barrier);
    }

    func(images[imageIndex]);

    {
        VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = 0,
            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = static_cast<VkImage>(images[imageIndex].get_handler()),
            .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                  .baseMipLevel = 0,
                                  .levelCount = 1,
                                  .baseArrayLayer = 0,
                                  .layerCount = 1 },
        };

        vkCmdPipelineBarrier(cb.first,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &barrier);
    }

    call_vulkan(vkEndCommandBuffer(cb.first));

    {
        VkSubmitInfo info = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                              .pNext = nullptr,
                              .waitSemaphoreCount = 0,
                              .pWaitSemaphores = nullptr,
                              .pWaitDstStageMask = nullptr,
                              .commandBufferCount = 1,
                              .pCommandBuffers = &cb.first,
                              .signalSemaphoreCount = 0,
                              .pSignalSemaphores = nullptr };

        call_vulkan(vkQueueSubmit(vkQueue, 1, &info, cb.second));
    }

    {
        struct VkPresentInfoKHR info = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                         .pNext = nullptr,
                                         .waitSemaphoreCount = 0,
                                         .pWaitSemaphores = nullptr,
                                         .swapchainCount = 1,
                                         .pSwapchains = &swapchain,
                                         .pImageIndices = &imageIndex,
                                         .pResults = nullptr };

        auto err = vkQueuePresentKHR(vkQueue, &info);

        if (err == VK_ERROR_OUT_OF_DATE_KHR)
        {
            cout << "vkQueuePresentKHR returned VK_OUT_OF_DATE_KHR" << endl;
        }
        else if (err == VK_SUBOPTIMAL_KHR)
        {
            cout << "vkQueuePresentKHR returned VK_SUBOPTIMAL_KHR" << endl
                 << "Probably the window has been resized." << endl;
            destroy_swapchain();
            create_swapchain();
        }
        else
        {
            call_vulkan(err);
        }
    }

    call_vulkan(vkResetFences(vkDevice, 1, &fence));
}

void sdl_window_vulkan::create_swapchain()
{
    VkSurfaceCapabilitiesKHR capabilities;
    call_vulkan(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(get_vulkan_physical_device(device), surface, &capabilities));

    {
        VkSwapchainCreateInfoKHR info = { .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                          .pNext = nullptr,
                                          .flags = 0,
                                          .surface = this->surface,
                                          .minImageCount = 2,
                                          .imageFormat = format.format,
                                          .imageColorSpace = format.colorSpace,
                                          .imageExtent = capabilities.currentExtent,
                                          .imageArrayLayers = 1,
                                          .imageUsage = VK_IMAGE_USAGE_STORAGE_BIT,
                                          .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                          .queueFamilyIndexCount = 0,
                                          .pQueueFamilyIndices = nullptr,
                                          .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
                                          .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                          .presentMode = VK_PRESENT_MODE_FIFO_KHR,
                                          .clipped = false,
                                          .oldSwapchain = nullptr };

        call_vulkan(vkCreateSwapchainKHR(vkDevice, &info, nullptr, &swapchain));
    }

    {
        uint32_t count;
        call_vulkan(vkGetSwapchainImagesKHR(vkDevice, swapchain, &count, nullptr));
        vector<VkImage> images(count);
        call_vulkan(vkGetSwapchainImagesKHR(vkDevice, swapchain, &count, images.data()));
        cout << "Number of swapchain images: " << count << endl;
        commandBuffers.resize(count);

        for (unsigned int k = 0; k < count; ++k)
        {
            this->images.push_back(image_buffer<2, Eigen::Vector<uint8_t, 4>, true>::create_from_vulkan(
                device,
                images[k],
                { capabilities.currentExtent.width, capabilities.currentExtent.height },
                format.format));

            {
                VkCommandBufferAllocateInfo info;
                info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                info.pNext = nullptr;
                info.commandPool = commandPool;
                info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                info.commandBufferCount = 1;

                call_vulkan(vkAllocateCommandBuffers(vkDevice, &info, &commandBuffers[k].first));
            }
            {
                VkFenceCreateInfo info = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                           .pNext = nullptr,
                                           .flags = VK_FENCE_CREATE_SIGNALED_BIT };

                call_vulkan(vkCreateFence(vkDevice, &info, nullptr, &commandBuffers[k].second));
            }
        }
    }
}

void sdl_window_vulkan::destroy_swapchain()
{
    for (auto& cb : commandBuffers)
    {
        call_vulkan(vkWaitForFences(vkDevice, 1, &cb.second, false, timeout));
        vkDestroyFence(vkDevice, cb.second, nullptr);
        vkFreeCommandBuffers(vkDevice, commandPool, 1, &cb.first);
    }
    commandBuffers.clear();
    images.clear();
    vkDestroySwapchainKHR(vkDevice, swapchain, nullptr);
}

sdl_window_vulkan::sdl_window_vulkan(const char* name, Eigen::Vector<Tuint, 2> size, uint32_t flags)
    : sdl_window(name, size, flags | SDL_WINDOW_VULKAN, nullptr)
{
    vector<const char*> extensions;

    {
        uint32_t count;
        const char* const* names = SDL_Vulkan_GetInstanceExtensions(&count);
        if (names == nullptr)
        {
            throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed");
        }

        extensions.assign(names, names + count);
    }

    cout << "Getting devices." << endl;
    vector<goopax_device> devices = get_devices_from_vulkan(nullptr, extensions, { "VK_KHR_swapchain" });

    cout << "devices.size()=" << devices.size() << endl;

    if (devices.empty())
    {
        throw std::runtime_error("Failed to find vulkan devices");
    }
    this->instance = get_vulkan_instance(devices[0]);

    auto vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();

#define setfunc(FUNC) this->FUNC = (PFN_##FUNC)(vkGetInstanceProcAddr(instance, #FUNC))

    setfunc(vkGetPhysicalDeviceSurfaceSupportKHR);
    setfunc(vkCreateSwapchainKHR);
    setfunc(vkGetSwapchainImagesKHR);
    setfunc(vkGetPhysicalDeviceSurfaceFormatsKHR);
    setfunc(vkCreateFence);
    setfunc(vkDestroyFence);
    setfunc(vkAllocateCommandBuffers);
    setfunc(vkFreeCommandBuffers);
    setfunc(vkCreateCommandPool);
    setfunc(vkDestroyCommandPool);
    setfunc(vkAcquireNextImageKHR);
    setfunc(vkDestroySwapchainKHR);
    setfunc(vkQueuePresentKHR);
    setfunc(vkCmdPipelineBarrier);
    setfunc(vkBeginCommandBuffer);
    setfunc(vkEndCommandBuffer);
    setfunc(vkQueueSubmit);
    setfunc(vkWaitForFences);
    setfunc(vkResetFences);
    setfunc(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);

#undef setfunc

    call_sdl(SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface));

    for (auto& device : devices)
    {
        uint32_t queueFamilyIndex = get_vulkan_queue_family_index(device);

        VkBool32 supported;

        call_vulkan(vkGetPhysicalDeviceSurfaceSupportKHR(
            get_vulkan_physical_device(device), queueFamilyIndex, surface, &supported));

        cout << "have device: " << device.name() << ". supported=" << supported;

        if (supported && !this->device.valid())
        {
            cout << ". Using.";
            this->device = device;
            vkDevice = static_cast<VkDevice>(device.get_device_ptr());
            vkQueue = static_cast<VkQueue>(device.get_device_queue());
        }
        cout << endl;
    }
    cout << endl;

    if (!this->device.valid())
    {
        throw std::runtime_error("Failed to find usable vulkan device");
    }

    vector<VkSurfaceFormatKHR> formats;
    {
        uint32_t count;
        call_vulkan(vkGetPhysicalDeviceSurfaceFormatsKHR(get_vulkan_physical_device(device), surface, &count, nullptr));
        formats.resize(count);
        call_vulkan(
            vkGetPhysicalDeviceSurfaceFormatsKHR(get_vulkan_physical_device(device), surface, &count, formats.data()));

        cout << "number of formats: " << count << endl;
        for (unsigned int k = 0; k < count; ++k)
        {
            cout << k << ": format=" << formats[k].format << ", colorSpace=" << formats[k].colorSpace << endl;
        }
    }

    this->format = formats[0];
    cout << "using format=" << format.format << ", colorSpace=" << format.colorSpace << endl;

    {
        VkCommandPoolCreateInfo info = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                         .pNext = nullptr,
                                         .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                         .queueFamilyIndex = get_vulkan_queue_family_index(device) };

        call_vulkan(vkCreateCommandPool(vkDevice, &info, nullptr, &this->commandPool));
    }

    create_swapchain();

    {
        VkFenceCreateInfo info = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = nullptr, .flags = 0 };
        call_vulkan(vkCreateFence(vkDevice, &info, nullptr, &fence));
    }
}

sdl_window_vulkan::~sdl_window_vulkan()
{
    destroy_swapchain();
    vkDestroyFence(vkDevice, fence, nullptr);
    vkDestroyCommandPool(vkDevice, commandPool, nullptr);
    SDL_Vulkan_DestroySurface(instance, surface, nullptr);
}
