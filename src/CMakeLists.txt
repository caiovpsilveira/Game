target_sources(voxel_game PRIVATE
               main.cpp)

target_link_libraries(voxel_game PRIVATE glm SDL2 VulkanMemoryAllocator)
