#include "Utils.hpp"

namespace core::utils
{

bool containsExtension(std::span<const char* const> extensionsList, const char* extensionName) noexcept
{
    return std::ranges::find(extensionsList, extensionName) != extensionsList.end();
}

bool containsExtension(std::span<const vk::ExtensionProperties> extensionsPropertiesList,
                       const char* extensionName) noexcept
{
    auto match = [extensionName](const vk::ExtensionProperties& p) {
        return std::strcmp(p.extensionName, extensionName) == 0;
    };

    auto result = std::ranges::find_if(extensionsPropertiesList, match);
    return result != extensionsPropertiesList.end();
}

}   // namespace core::utils
