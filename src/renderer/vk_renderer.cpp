#include <vulkan/vulkan.h>

#ifdef WINDOWS_BUILD
#include <vulkan/vulkan_win32.h>
#elif LINUX_BUILD
#endif

#include <iostream>

#define ArraySize(arr) sizeof((arr)) / sizeof((arr[0]))

#define VK_CHECK(result)                                      \
    if (result != VK_SUCCESS)                                 \
    {                                                         \
        std::cout << "Vulkan error: " << result << std::endl; \
        __debugbreak();                                       \
        return false;                                         \
    };

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT msgSeverity,
    VkDebugUtilsMessageTypeFlagsEXT msgFlags,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData)
{
    std::cout << "Validation error: " << pCallbackData->pMessage << std::endl;
    return false;
}

struct VkContext
{
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkSwapchainKHR swapchain;
    VkDebugUtilsMessengerEXT debugMessenger;

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
#ifdef WINDOWS_BUILD
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif LINUX_BUILD
#endif
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
        debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
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
#ifdef WINDOWS_BUILD
        VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
        surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surfaceInfo.hwnd = (HWND)window;
        surfaceInfo.hinstance = GetModuleHandleA(0);
        VK_CHECK(vkCreateWin32SurfaceKHR(vkcontext->instance, &surfaceInfo, 0, &vkcontext->surface));
#elif LINUX_BUILD
#endif
    }

    // Choose GPU
    {
        // Query the GPU
        // TODO: Sub-allocation from the main allocation
        vkcontext->graphicsIdx = -1;
        VkPhysicalDevice gpus[10];
        uint32_t gpuCount = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(vkcontext->instance, &gpuCount, 0));
        VK_CHECK(vkEnumeratePhysicalDevices(vkcontext->instance, &gpuCount, gpus));

        for (uint32_t i = 0; i < gpuCount; i++)
        {
            VkPhysicalDevice gpu = gpus[i];

            uint32_t queueFamilyCount = 0;
            // TODO: Sub-allocation from the main allocation
            VkQueueFamilyProperties queueProps[10];
            // std::vector<VkQueueFamilyProperties> sdaddad    // Another way?
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, 0);
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
                    break;
                }
            }
        };
    }

    if (vkcontext->graphicsIdx < 0)
    {
        return false;
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
    }

    // Swapchain
    {
        VkSwapchainCreateInfoKHR scInfo = {};
        scInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        scInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        scInfo.surface = vkcontext->surface;

        VK_CHECK(vkCreateSwapchainKHR(vkcontext->device, &scInfo, 0, &vkcontext->swapchain))
    }

    return true;
}
