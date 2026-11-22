#pragma once

#include <cstdint>

struct InputState {
    bool keys[256];
    int16_t mouseDX;
    int16_t mouseDY;
    bool mouseButtons[5];
};

class IPlatform {
public:
    virtual ~IPlatform() = default;

    virtual bool InitWindow(uint32_t width, uint32_t height, const char* title) = 0;
    virtual void PollEvents() = 0;
    virtual bool ShouldQuit() const = 0;
    virtual double GetTime() const = 0;
    virtual InputState GetInputState() const = 0;
    virtual void SetWindowShouldCloseCallback(bool (*callback)(bool)) = 0;
    virtual void SetWindowTitle(const char* title) = 0;
    virtual void ShutdownWindow() = 0;
    virtual const char* GetSaveDir() const = 0;
    virtual void SetWindowIcon(const uint8_t* pixels, uint32_t width, uint32_t height) = 0;

    // Callback for when should quit is requested
    using CloseCallback = bool (*)(bool);
    virtual void SetCloseCallback(CloseCallback cb) = 0;
};