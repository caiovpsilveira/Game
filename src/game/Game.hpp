#ifndef GAME_GAME_HPP
#define GAME_GAME_HPP

#include "renderer/Renderer.hpp"

struct SDL_Window;

namespace game
{

class Game
{
public:
    Game();

    ~Game() noexcept;

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    Game(Game&&) = delete;
    Game& operator=(Game&&) = delete;

public:
    void run();

private:
    SDL_Window* m_window;
    renderer::Renderer m_renderer;
};

}   // namespace game

#endif
