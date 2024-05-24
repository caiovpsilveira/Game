#include <SDL.h>
#include <expected>
#include <glm/glm.hpp>
#include <iostream>
#include <string>
#include <vk_mem_alloc.h>

struct S {
    int a;
    int b;
};

int main() {
    std::cout << "Test" << std::endl;

    struct S testCpp20 {
        .a = 10, .b = 20
    };

    std::expected<int, std::string> test {};

    auto v = SDL_TRUE;

    auto vec = glm::vec3();

    VmaAllocator allocator;

    return 0;
}
