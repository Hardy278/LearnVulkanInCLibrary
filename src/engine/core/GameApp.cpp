#include "GameApp.hpp"
#include "../render/VulkanRenderer.hpp"
#include "Time.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <spdlog/spdlog.h>

#include <stdexcept>
#include <string>

namespace engine::core {
GameApp::GameApp() = default;

GameApp::~GameApp() {
    if (m_isRunning) {
        spdlog::error("GameApp::游戏未正常关闭，请检查代码");
        close();
    }
}

void GameApp::run() {
    if (!init()) {
        spdlog::error("GameApp::run()::GameApp 初始化失败");
        return;
    }
    while (m_isRunning) {
        m_time->update();
        float deltaTime = static_cast<float>(m_time->getDeltaTime());
        handleEvents();
        if (!m_isMinimized) {
            update(deltaTime);
            render();
        }
        // spdlog::info("FPS: {}", 1.0f / deltaTime);
    }
    close();
}

void GameApp::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            m_isRunning = false;
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            spdlog::info("GameApp::handleEvents()::窗口大小改变");
            m_renderer->setFramebufferResized(true);
            break;
        case SDL_EVENT_WINDOW_MINIMIZED:
            spdlog::info("GameApp::handleEvents()::窗口最小化");
            m_isMinimized = true;
            break;
        case SDL_EVENT_WINDOW_RESTORED:
            spdlog::info("GameApp::handleEvents()::窗口恢复");
            m_isMinimized = false;
            break;
        default:
            break;
        }
    }
}

void GameApp::update(float deltaTime) {
}

void GameApp::render() {
    m_renderer->render();
}

void GameApp::close() {
    spdlog::trace("GameApp::close()::关闭 GameApp...");
    m_renderer->cleanup(); // 手动清理 VulkanRenderer 因为析构函数里没有清理
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
    m_isRunning = false;
}

bool GameApp::init() {
    spdlog::trace("GameApp::init()::初始化 GameApp...");
    if (!initWindow()) return false;
    if (!initVulkanRenderer()) return false;
    if (!initTime()) return false;
    m_isRunning = true;
    spdlog::trace("GameApp::init()::初始化成功");
    return true;
}

bool GameApp::initWindow() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        spdlog::error("GameApp::initWindow()::SDL初始化失败: {}", SDL_GetError());
        return false;
    }
    // 检查Vulkan支持
    if (!SDL_Vulkan_LoadLibrary(nullptr)) {
        spdlog::error("GameApp::initWindow()::无法加载Vulkan库: {}", SDL_GetError());
        return false;
    }
    m_window = SDL_CreateWindow("GameApp", WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
    if (!m_window) {
        spdlog::error("GameApp::initWindow()::SDL窗口创建失败: {}", SDL_GetError());
        return false;
    }
    spdlog::trace("GameApp::initWindow()::SDL窗口创建成功");
    return true;
}

bool GameApp::initVulkanRenderer() {
    try {
        m_renderer = std::make_unique<engine::render::VulkanRenderer>(m_window);
    } catch (const std::exception &e) {
        spdlog::error("GameApp::initVulkanRenderer()::VulkanRenderer初始化失败: {}", e.what());
        return false;
    }
    spdlog::trace("GameApp::initVulkanRenderer()::VulkanRenderer初始化成功");
    return true;
}

bool GameApp::initTime() {
    try {
        m_time = std::make_unique<Time>(60);
    } catch (const std::exception &e) {
        spdlog::error("GAME::initTime::时间管理器初始化失败: {}", e.what());
        return false;
    }
    m_time->setTargetFPS(60);
    spdlog::trace("GAME::initTime::时间管理器初始化成功, FPS: {}", 60);
    return true;
}

} // namespace engine::core