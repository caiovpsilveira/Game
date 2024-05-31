# Voxel game

This is my attempt to make a voxel game to learn Vulkan.

I'm using C++20 because of designated initializers, std::span, std::format, std::ranges and other nice features from C++20.

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
Check [here](https://en.cppreference.com/w/cpp/compiler_support#C.2B.2B20_library_features) to see which version of your compiler supports a certain feature.

For Clang, I had to add `-stdlib=libc++` to `CMAKE_CXX_FLAGS` for it to recognize std::format, and install `libc++-dev`. I tested with Clang 18.1.4.

Alternative, there is a `cmake.sh` script in the project root, which sets the compiler as Clang and overwrites some CMake rules with `CMakeOverrides.txt`, such as compile flags and setting `CMAKE_EXPORT_COMPILE_COMMANDS` for clangd.

Note that there are some target compile definitions set in the `src/CMakeLists,txt`, which tells Vulkan.hpp and VMA to use dynamic entrypoints, which are loaded in the constructor of `VulkanGraphicsContext`.
