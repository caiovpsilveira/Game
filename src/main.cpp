#include "core/Logger.hpp"
#include "game/Game.hpp"

#include <exception>

int main()
{
    try {
        game::Game game;
        game.run();
    } catch (const std::exception& e) {
        FATAL_FMT("{}\n", e.what());
        return 1;
    }

    return 0;
}
