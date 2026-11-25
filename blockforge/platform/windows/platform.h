#pragma once

#include "common/iplatform.h"
#include <GLFW/glfw3.h>

class WindowsPlatform : public IPlatform {
public:
    WindowsPlatform();
    ~WindowsPlatform() override;

    bool InitWindow(uint32_t width, uint32_t height, const char* title) override;
    void PollEvents() override;
    void SwapBuffers() override;
    bool ShouldQuit() const override;
    double GetTime() const override;
    InputState GetInputState() const override;
    void SetWindowShouldCloseCallback(bool (*callback)(bool)) override;
    void SetCloseCallback(CloseCallback cb) override;
    void SetWindowTitle(const char* title) override;
    void SetCursorCaptured(bool captured) override;
    void ShutdownWindow() override;
    const char* GetSaveDir() const override;
    void SetWindowIcon(const uint8_t* pixels, uint32_t width, uint32_t height) override;

private:
    GLFWwindow* m_window = nullptr;
    bool m_shouldQuit = false;
    CloseCallback m_closeCallback = nullptr;
    InputState m_inputState{};
    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;
    int32_t m_accumMouseDX = 0;
    int32_t m_accumMouseDY = 0;

    void OnKey(int key, int action);
    void OnCursor(double x, double y);
    void OnMouseButton(int button, int action);
};
