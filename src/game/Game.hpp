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

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    Game(Game&&) = delete;
    Game& operator=(Game&&) = delete;

public:
    void run();

private:
    void createGraphicsPipeline();

private:
    SDL_Window* m_window;
    core::VulkanGraphicsContext m_vkContext;
    vk::UniquePipeline m_graphicsPipeline;
};

}   // namespace game

#endif
