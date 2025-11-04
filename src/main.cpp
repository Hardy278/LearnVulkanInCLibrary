#include "engine/core/GameApp.hpp"
#include <spdlog/spdlog.h>

int main() {
    spdlog::set_level(spdlog::level::trace);
    engine::core::GameApp game;
    game.run();
    return 0;
}