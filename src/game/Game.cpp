#include "Game.hpp"

// libs
#include <SDL.h>

namespace game
{

Game::Game()
{
    SDL_Init(SDL_INIT_VIDEO);
    m_window = SDL_CreateWindow("Unnamed game", 0, 0, 800, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
    m_renderer = renderer::Renderer(m_window);
}

Game::~Game() noexcept
{
    SDL_DestroyWindow(m_window);
    SDL_Quit();
}

void Game::run()
{
    bool quit = false;
    SDL_Event e;
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
                break;
            }
        }
        m_renderer.drawFrame();
    }
}

}   // namespace game
