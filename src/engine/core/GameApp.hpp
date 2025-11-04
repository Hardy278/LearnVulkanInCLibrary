#pragma once
#include <SDL3/SDL.h>
#include <SDL3/SDL_mutex.h>
#include <memory>

#pragma region Constants
const uint32_t WIDTH  = 800;
const uint32_t HEIGHT = 600;
#pragma endregion

#pragma region Forward Declaration
struct SDL_Window;

namespace engine::render {
class VulkanRenderer;
}
#pragma endregion

namespace engine::core {
class Time;

class GameApp final {
public:
    GameApp();
    ~GameApp();

    GameApp(const GameApp &)            = delete;
    GameApp &operator=(const GameApp &) = delete;
    GameApp(GameApp &&)                 = delete;
    GameApp &operator=(GameApp &&)      = delete;

    void run();

private:
#pragma region Menber Variables
    SDL_Window *m_window;       // SDL windows窗口句柄
    bool m_isMinimized = false; // 游戏是否最小化
    bool m_isRunning   = false; // 游戏是否运行

    std::unique_ptr<engine::render::VulkanRenderer> m_renderer; // 渲染器
    std::unique_ptr<engine::core::Time> m_time;                 // 时间管理器
#pragma endregion

    void handleEvents();          // 处理 SDL 事件
    void update(float deltaTime); // 更新游戏状态
    void render();                // 渲染游戏画面
    void close();                 // 关闭 SDL 窗口和渲染器，释放资源

#pragma region Initialization
    [[nodiscard]] bool init();
    [[nodiscard]] bool initWindow();
    [[nodiscard]] bool initVulkanRenderer();
    [[nodiscard]] bool initTime();
#pragma endregion
};

} // namespace engine::core