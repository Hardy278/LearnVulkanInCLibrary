#pragma once
#include <SDL3/SDL_stdinc.h>

namespace engine::core {

class Time final {
private:
    Uint64 m_startTime = 0;   // 开始时间
    Uint64 m_endTime   = 0;   // 结束时间
    double m_deltaTime = 0.0; // 时间间隔
    double m_timeScale = 1.0; // 时间缩放

    int m_targrtFPS          = 60;  // 目标帧率
    double m_targetFrameTime = 0.0; // 帧时间

public:
    Time();
    Time(int fps);
    ~Time();

    Time(const Time &)            = delete;
    Time &operator=(const Time &) = delete;
    Time(Time &&)                 = delete;
    Time &operator=(Time &&)      = delete;

    // 更新时间
    void update();

    void setTargetFPS(int fps) {
        m_targrtFPS       = fps;
        m_targetFrameTime = 1.0f / fps;
    }
    void setTimeScale(double scale) { m_timeScale = scale; }

    double getDeltaTime() const { return m_deltaTime * m_timeScale; }
    double getUnscaledDeltaTime() const { return m_deltaTime; }
    double getTimeScale() const { return m_timeScale; }
    double getFrameTime() const { return m_targetFrameTime; }

private:
    void limitFrameRate(float currentDeltaTime);
};

} // namespace engine::core