#include <SDL.h>
#include <glm/glm.hpp>
#include <iostream>

struct S {
    int a;
    int b;
};

int main() {
    std::cout << "Test" << std::endl;

    struct S testCpp20 {
        .a = 10, .b = 20
    };

    auto v = SDL_TRUE;

    auto vec = glm::vec3();

    return 0;
}
