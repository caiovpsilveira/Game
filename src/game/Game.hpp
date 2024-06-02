#ifndef GAME_GAME_HPP
#define GAME_GAME_HPP

#include "core/VulkanGraphicsContext.hpp"

struct SDL_Window;

namespace game
{

class Game
{
public:
    Game();

    ~Game() noexcept;

    void run();

private:
    void createGraphicsPipeline();

private:
    SDL_Window* m_window;
    core::VulkanGraphicsContext m_vkContext;
};

}   // namespace game

#endif
