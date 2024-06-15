#ifndef GAME_GAME_HPP
#define GAME_GAME_HPP

#include "renderer/Types.hpp"
#include "renderer/VulkanGraphicsContext.hpp"

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
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    void createGraphicsPipeline();
    void initTransferData();
    void initFrameData();

    void uploadMesh();

    void drawFrame();

private:
    SDL_Window* m_window;
    renderer::VulkanGraphicsContext m_vkContext;
    vk::UniquePipeline m_graphicsPipeline;
    renderer::TransferData m_transferData;
    size_t m_frameCount = 0;
    renderer::FrameData m_frameData[MAX_FRAMES_IN_FLIGHT];
    renderer::Mesh m_testMesh;
};

}   // namespace game

#endif
