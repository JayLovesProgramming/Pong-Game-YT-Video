#include <vulkan/vulkan.h>
#include <iostream>

int main()
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

    // Vulkan Instance
    VkInstance instance;

    // Creates the vulkan instance
    VkResult result = vkCreateInstance(&instanceInfo, 0, &instance);
    if (result == VK_SUCCESS)
    {
        std::cout << "Successfully created VK instance" << std::endl;
    }
    return 0;
}