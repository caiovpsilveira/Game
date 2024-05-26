# Voxel game

This is my attempt to make a voxel game to learn Vulkan.

I'm using C++23 because of designated initializers, std::span, std::format, std::ranges and other nice features from C++20/23.

## Dependencies
For third_party libraries, I'm using [SDL2](https://www.libsdl.org/), [GLM](https://github.com/g-truc/glm). These are already embedded within the project.

For external dependencies, this requires the Vulkan SDK. You can download the latest Vulkan SDK [here](https://www.lunarg.com/vulkan-sdk/).
Vulkan SDK is required for both Vulkan and [VulkanMemoryAllocator](https://gpuopen.com/vulkan-memory-allocator/), which is shipped with the Vulkan SDK since 1.3.216.0.

## Building
This project is configured with CMake. In-source builds are disabled.

```
mkdir build
cd build
cmake ..
make
```

This was tested with GNU and Clang.
For GNU, just setting `CMAKE_CXX_STANDARD 23` in the CMakeLists.txt worked. GNU 14 is required for runtime format string support.

For Clang, I had to add `-stdlib=libc++` to `CMAKE_CXX_FLAGS` for it to recognize std::expected, and install `libc++-dev`. I tested with Clang 18.1.4.

You may need to check how to enable C++20/23 features with your compiler.

Alternative, there is a `cmake.sh` script in the project root, which sets the compiler as Clang and overwrites some CMake rules with `CMakeOverrides.txt`, such as compile flags and `CMAKE_EXPORT_COMPILE_COMMANDS`.
