#include <iostream>
#include <vulkan/vulkan.h>
#include <windows.h>
#include "vk_init.cpp"

using namespace std;

// #ifdef WINDOWS_BUILD
#include <vulkan/vulkan_win32.h> // Only include this if on windows build
// #elif LINUX_BUILD
// Use another library if we are on Linux
// #endif

void handleVulkanError(VkResult result)
{
    switch (result)
    {
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        cout << "ERROR: Out of host memory!" << endl;
        break;
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        cout << "ERROR: Out of device memory!" << endl;
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        cout << "ERROR: Initialization failed!" << endl;
        break;
    case VK_ERROR_SURFACE_LOST_KHR:
        cout << "ERROR: Surface lost!" << endl;
        break;
    case VK_SUCCESS:
        // No error
        cout << "No Vulkan Errors ✅";

        return;
    default:
        cout << "ERROR: Unexpected error: " << result << endl;
        cout << "\033[31mERROR: Unexpected error: " << result << "\033[0m" << endl;
        break;
    }
}

#define ArraySize(arr) sizeof((arr)) / sizeof((arr[0]))

#define VK_CHECK(result)                            \
    if (result != VK_SUCCESS)                       \
    {                                               \
        cout << "Vulkan error: " << result << endl; \
        __debugbreak();                             \
        return false;                               \
    }

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT msgSeverity,
    VkDebugUtilsMessageTypeFlagsEXT msgFlags,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData)
{
    cout << "Validation error: " << pCallbackData->pMessage << endl;
    return false;
}

struct VkContext
{
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkSwapchainKHR swapchain;
    VkSurfaceFormatKHR surfaceFormat;
    VkQueue graphicsQueue;
    VkCommandPool commandPool;
    VkSemaphore submitSemaphore;
    VkSemaphore acquireSemaphore;

    uint32_t scImgCount;
    // Sub allocation from main allocation memory
    VkImage scImages[5];

    int graphicsIdx;
};

bool vk_init(VkContext *vkcontext, void *window)
{
    // Application Info
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Jay's Ping Pong From Scratch";
    appInfo.pEngineName = "Pong Engine";

    char *extensions[] = {
        // #ifdef WINDOWS_BUILD
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        // #elif LINUX_BUILD
        // #endif
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME};

    char *layers[]{
        "VK_LAYER_KHRONOS_validation"};

    // More Application Info
    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.ppEnabledExtensionNames = extensions;
    instanceInfo.enabledExtensionCount = ArraySize(extensions);
    instanceInfo.ppEnabledLayerNames = layers;
    instanceInfo.enabledLayerCount = ArraySize(layers);

    VK_CHECK(vkCreateInstance(&instanceInfo, 0, &vkcontext->instance));

    auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkcontext->instance, "vkCreateDebugUtilsMessengerEXT");
    if (vkCreateDebugUtilsMessengerEXT)
    {
        VkDebugUtilsMessengerCreateInfoEXT debugInfo = {};
        debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        // debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debugInfo.pfnUserCallback = vk_debug_callback;
        vkCreateDebugUtilsMessengerEXT(vkcontext->instance, &debugInfo, 0, &vkcontext->debugMessenger);
    }
    else
    {
        return false;
    }

    // Create surface
    {
        VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
        surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surfaceInfo.hwnd = (HWND)window;
        surfaceInfo.hinstance = GetModuleHandleA(0);
        VK_CHECK(vkCreateWin32SurfaceKHR(vkcontext->instance, &surfaceInfo, 0, &vkcontext->surface));
    }

    // Choose GPU
    {
        // Query the GPU
        // TODO: Sub-allocation from the main allocation
        vkcontext->graphicsIdx = -1;
        uint32_t gpuCount = 0;
        VkPhysicalDevice gpus[10];
        VK_CHECK(vkEnumeratePhysicalDevices(vkcontext->instance, &gpuCount, nullptr));
        VK_CHECK(vkEnumeratePhysicalDevices(vkcontext->instance, &gpuCount, gpus));

        for (uint32_t i = 0; i < gpuCount; i++)
        {
            VkPhysicalDevice gpu = gpus[i];
            uint32_t queueFamilyCount = 0;
            VkQueueFamilyProperties queueProps[10];

            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, nullptr);
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, queueProps);

            for (uint32_t j = 0; j < queueFamilyCount; j++)
            {
                if (queueProps[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    VkBool32 surfaceSupport = VK_FALSE;
                    VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(gpu, j, vkcontext->surface, &surfaceSupport));
                    if (surfaceSupport)
                    {
                        vkcontext->graphicsIdx = j;
                        vkcontext->gpu = gpu;
                        break;
                    }
                }
            }
        }

        if (vkcontext->graphicsIdx < 0)
        {
            cout << "Failed to find a suitable GPU!" << endl;
            return false;
        }
    }

    // Logical Device
    {
        float queuePriority = 1.0f;

        VkDeviceQueueCreateInfo queueInfo = {};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = vkcontext->graphicsIdx;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;

        char *extensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        VkDeviceCreateInfo deviceInfo = {};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.ppEnabledExtensionNames = extensions;
        deviceInfo.enabledExtensionCount = ArraySize(extensions);

        VK_CHECK(vkCreateDevice(vkcontext->gpu, &deviceInfo, 0, &vkcontext->device));
        vkGetDeviceQueue(vkcontext->device, vkcontext->graphicsIdx, 0, &vkcontext->graphicsQueue);
    }

    // Swapchain
    {
        uint32_t formatCount = 0;
        // TODO: Sub-allocation from main allocation
        VkSurfaceFormatKHR surfaceFormats[10];
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkcontext->gpu, vkcontext->surface, &formatCount, 0));
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkcontext->gpu, vkcontext->surface, &formatCount, surfaceFormats));

        for (uint32_t i = 0; i < formatCount; i++)
        {
            VkSurfaceFormatKHR format = surfaceFormats[i];
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB)
            {
                vkcontext->surfaceFormat = format;
                break;
            }
        }

        VkSurfaceCapabilitiesKHR surfaceCaps = {};
        VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkcontext->gpu, vkcontext->surface, &surfaceCaps); // Why tf doesn't this pass validation

        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(vkcontext->gpu, &deviceProperties);
        cout << "Running on GPU: " << deviceProperties.deviceName << endl;
        cout << "GPU: " << vkcontext->gpu << endl;
        cout << "CPU: " << vkcontext->device << endl;
        cout << "Surface: " << vkcontext->surface << endl;

        if (result != VK_SUCCESS)
        {
            handleVulkanError(result); // Call the error handling function
            return false;
        }

        uint32_t imgCount = surfaceCaps.minImageCount + 1;
        imgCount = imgCount > surfaceCaps.maxImageCount ? imgCount - 1 : imgCount;

        VkSwapchainCreateInfoKHR scInfo = {};
        scInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        scInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        scInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        scInfo.surface = vkcontext->surface;
        scInfo.preTransform = surfaceCaps.currentTransform;
        scInfo.imageExtent = surfaceCaps.currentExtent;
        scInfo.minImageCount = imgCount;
        scInfo.imageArrayLayers = 1;
        scInfo.imageFormat = vkcontext->surfaceFormat.format;

        VK_CHECK(vkCreateSwapchainKHR(vkcontext->device, &scInfo, 0, &vkcontext->swapchain));

        VK_CHECK(vkGetSwapchainImagesKHR(vkcontext->device, vkcontext->swapchain, &vkcontext->scImgCount, 0));
        VK_CHECK(vkGetSwapchainImagesKHR(vkcontext->device, vkcontext->swapchain, &vkcontext->scImgCount, vkcontext->scImages));
    }

    // Command Pool
    {
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = vkcontext->graphicsIdx;
        VK_CHECK(vkCreateCommandPool(vkcontext->device, &poolInfo, 0, &vkcontext->commandPool));
    }

    // Sync objects
    {
        VkSemaphoreCreateInfo semaInfo = {};
        semaInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_CHECK(vkCreateSemaphore(vkcontext->device, &semaInfo, 0, &vkcontext->acquireSemaphore));
        VK_CHECK(vkCreateSemaphore(vkcontext->device, &semaInfo, 0, &vkcontext->submitSemaphore));
    }
    return true;
}

bool vk_render(VkContext *vkcontext)
{
    uint32_t imgIndex;

    VK_CHECK(vkAcquireNextImageKHR(vkcontext->device, vkcontext->swapchain, 0, vkcontext->acquireSemaphore, 0, &imgIndex));

    VkCommandBuffer cmd;

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandBufferCount = 1;
    allocInfo.commandPool = vkcontext->commandPool;
    VK_CHECK(vkAllocateCommandBuffers(vkcontext->device, &allocInfo, &cmd));

    VkCommandBufferBeginInfo beginInfo = cmd_begin_info();
    // Begin command buffer
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    // Rendering commands
    {

        VkClearColorValue color = {255, 255, 0, 155};
        VkImageSubresourceRange range = {};
        range.layerCount = 1;
        range.levelCount = 1;
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        vkCmdClearColorImage(cmd, vkcontext->scImages[imgIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, &color, 1, &range);
    }

    // End command buffer
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.pCommandBuffers = &cmd;
    // Submit semaphores // Signals only on submit
    submitInfo.pSignalSemaphores = &vkcontext->submitSemaphore;
    submitInfo.signalSemaphoreCount = 1;
    // Wait for semaphore
    submitInfo.pWaitSemaphores = &vkcontext->acquireSemaphore;
    submitInfo.waitSemaphoreCount = 1;

    VK_CHECK(vkQueueSubmit(vkcontext->graphicsQueue, 1, &submitInfo, 0));

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pSwapchains = &vkcontext->swapchain;
    presentInfo.pImageIndices = &imgIndex;
    presentInfo.swapchainCount = 1;
    // Present semaphores
    presentInfo.pWaitSemaphores = &vkcontext->submitSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    VK_CHECK(vkQueuePresentKHR(vkcontext->graphicsQueue, &presentInfo));

    // VK_CHECK(vkDeviceWaitIdle(vkcontext->device));

    vkFreeCommandBuffers(vkcontext->device, vkcontext->commandPool, 1, &cmd);
    return true;
}