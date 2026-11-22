#pragma once

#include "common/iplatform.h"
#include <GLFW/glfw3.h>

class LinuxPlatform : public IPlatform {
public:
    LinuxPlatform();
    ~LinuxPlatform() override;

    bool InitWindow(uint32_t width, uint32_t height, const char* title) override;
    void PollEvents() override;
    bool ShouldQuit() const override;
    double GetTime() const override;
    InputState GetInputState() const override;
    void SetWindowShouldCloseCallback(bool (*callback)(bool)) override;
    void SetWindowTitle(const char* title) override;
    void ShutdownWindow() override;
    const char* GetSaveDir() const override;
    void SetWindowIcon(const uint8_t* pixels, uint32_t width, uint32_t height) override;

private:
    GLFWwindow* m_window = nullptr;
    bool m_shouldQuit = false;
    CloseCallback m_closeCallback = nullptr;
    InputState m_inputState{};
};