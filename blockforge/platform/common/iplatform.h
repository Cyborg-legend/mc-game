#pragma once

#include <cstdint>

// GLFW key codes run past 255 (arrows, Enter, Escape, Backspace), so the table
// has to cover the whole GLFW_KEY_LAST range.
static constexpr int INPUT_KEY_COUNT = 512;

struct InputState {
    bool keys[INPUT_KEY_COUNT];
    int16_t mouseDX;
    int16_t mouseDY;
    bool mouseButtons[5];
};

class IPlatform {
public:
    using CloseCallback = bool (*)(bool);

    virtual ~IPlatform() = default;

    virtual bool InitWindow(uint32_t width, uint32_t height, const char* title) = 0;
    virtual void PollEvents() = 0;
    virtual void SwapBuffers() = 0;
    virtual bool ShouldQuit() const = 0;
    virtual double GetTime() const = 0;
    virtual InputState GetInputState() const = 0;
    virtual void SetWindowShouldCloseCallback(bool (*callback)(bool)) = 0;
    virtual void SetCloseCallback(CloseCallback cb) = 0;
    virtual void SetWindowTitle(const char* title) = 0;
    // Grabs or releases the mouse: menus show a cursor, gameplay hides it.
    virtual void SetCursorCaptured(bool captured) = 0;
    virtual void ShutdownWindow() = 0;
    virtual const char* GetSaveDir() const = 0;
    virtual void SetWindowIcon(const uint8_t* pixels, uint32_t width, uint32_t height) = 0;
};

// Creates the platform backend for the current OS.
IPlatform* CreatePlatform();
