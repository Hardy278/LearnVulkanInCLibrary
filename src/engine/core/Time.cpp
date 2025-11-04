#include "Time.hpp"

#include <SDL3/SDL_Timer.h>
#include <spdlog/spdlog.h>

namespace engine::core {

Time::Time() {
    m_endTime = SDL_GetTicksNS();
    m_startTime = m_endTime;
    m_timeScale = 1.0;
}

Time::Time(int fps) {
    m_endTime = SDL_GetTicksNS();
    m_startTime = m_endTime;
    m_targrtFPS = fps;
    m_targetFrameTime = 1.0f / fps;
    m_timeScale = 1.0;
}

Time::~Time() {
    spdlog::trace("Time::退出成功");
}

void Time::update() {
    m_startTime = SDL_GetTicksNS();
    auto currentDeltaTime = static_cast<double>(m_startTime - m_endTime) / 1000000000.0; // 计算当前帧时间
    if (currentDeltaTime > 0.0) {
        limitFrameRate(static_cast<float>(currentDeltaTime)); // 如果设置了帧率限制，则限制帧率
    } else {
        m_deltaTime = currentDeltaTime; // 如果当前帧时间小于0，则直接使用当前帧时间
    }
    m_endTime = SDL_GetTicksNS(); // 更新结束时间
}

void Time::limitFrameRate(float currentDeltaTime) {
    if (currentDeltaTime < m_targetFrameTime) {
        double timeToWait = m_targetFrameTime - currentDeltaTime;
        Uint64 nsToWait = static_cast<Uint64>(timeToWait * 1000000000.0);
        SDL_DelayNS(nsToWait);
        m_deltaTime = static_cast<double>(SDL_GetTicksNS() - m_startTime) / 1000000000.0;
    }
}

} // namespace engine::core