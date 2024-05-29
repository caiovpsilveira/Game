target_sources(voxel_game PRIVATE
               core/Logger.cpp
               core/Utils.cpp
               core/VulkanGraphicsContext.cpp
               main.cpp)

target_compile_definitions(voxel_game PRIVATE
                           VK_NO_PROTOTYPES
                           VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
                           VMA_STATIC_VULKAN_FUNCTIONS=0
                           VMA_DYNAMIC_VULKAN_FUNCTIONS=0)

target_include_directories(voxel_game PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(voxel_game PRIVATE glm SDL2)
