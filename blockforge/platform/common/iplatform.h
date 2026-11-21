#pragma once

#include <string>
#include <cstdint>

struct InputState {
    bool keys[256];
    float mouseDX, mouseDY;
    bool mouseButtons[5];
};

struct IPlatform {
    virtual ~IPlatform() = default;
    virtual bool InitWindow(const char* title, int width, int height) = 0;
    virtual void PollEvents() = 0;
    virtual bool ShouldQuit() const = 0;
    virtual void* GetVulkanSurface() = 0;  // platform-specific handle
    virtual void* GetGLFWwindow() = 0;
    virtual void* GetVulkanInstance() = 0;
    virtual std::string GetSaveDir() = 0;
    virtual void SetWindowTitle(const char* title) = 0;
};

// Windows implementation
#if defined(_WIN32) || defined(__WIN32__)
#include <windows.h>

namespace platform::windows {

class Platform final : public IPlatform {
public:
    bool InitWindow(const char* title, int width, int height) override;
    void PollEvents() override;
    bool ShouldQuit() const override { return quit_; }
    void* GetVulkanSurface() override;
    void* GetGLFWwindow() override { return glfwWindow_; }
    void* GetVulkanInstance() override { return vulkanInstance_; }
    std::string GetSaveDir() override;
    void SetWindowTitle(const char* title) override;

    ~Platform() override { Cleanup(); }

private:
    void Cleanup();
    bool quit_ = false;
    void* glfwWindow_ = nullptr;
    void* vulkanInstance_ = nullptr;
    HCURSOR cursor_ = nullptr;
};

} // namespace platform::windows

// Linux implementation (GLFW with Wayland preferred, X11 fallback)
#else

namespace platform::linux {

class Platform final : public IPlatform {
public:
    bool InitWindow(const char* title, int width, int height) override;
    void PollEvents() override;
    bool ShouldQuit() const override { return quit_; }
    void* GetVulkanSurface() override;
    void* GetGLFWwindow() override { return glfwWindow_; }
    void* GetVulkanInstance() override { return vulkanInstance_; }
    std::string GetSaveDir() override;
    void SetWindowTitle(const char* title) override;

    ~Platform() override { Cleanup(); }

private:
    void Cleanup();
    bool quit_ = false;
    void* glfwWindow_ = nullptr;
    void* vulkanInstance_ = nullptr;
};

} // namespace platform::linux

// Factory
inline IPlatform* CreatePlatform() {
#ifdef _WIN32
    return new platform::windows::Platform();
#else
    return new platform::linux::Platform();
#endif
}

#endif