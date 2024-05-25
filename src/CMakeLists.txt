target_sources(voxel_game PRIVATE
               core/Logger.cpp
               core/VulkanContext.cpp
               main.cpp)

target_include_directories(voxel_game PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(voxel_game PRIVATE glm SDL2 volk VulkanMemoryAllocator)
