#include "core/VulkanContext.hpp"

#include <iostream>

int main() {

    auto vkContext = core::VulkanContext::createVulkanContext({}, true, true);

    if (!vkContext.has_value())
    {
        std::cerr << "Vulkan context creation returned with error " << vkContext.error() << std::endl;
        return -1;
    }

    return 0;
}
