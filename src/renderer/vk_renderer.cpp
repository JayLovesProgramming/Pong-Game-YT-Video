#include <vulkan/vulkan.h>

struct VkContext
{
    VkInstance instance;
};

bool vk_init(VkContext *vkcontext)
{
    // Application Info
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Jay's Ping Pong From Scratch";
    appInfo.pEngineName = "Pong Engine";

    // More Application Info
    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;

    // Creates the vulkan instance
    VkResult result = vkCreateInstance(&instanceInfo, 0, &vkcontext->instance);

    if (result == VK_SUCCESS)
    {
        return true;
    }
    return false;
}