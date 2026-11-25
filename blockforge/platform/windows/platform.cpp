#include "platform/windows/platform.h"
#include "platform/common/iplatform.h"

#include <GL/glew.h>

#include <windows.h>
#include <shlobj.h>

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

WindowsPlatform::WindowsPlatform() = default;
WindowsPlatform::~WindowsPlatform() = default;

bool WindowsPlatform::InitWindow(uint32_t width, uint32_t height, const char* title) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_window = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), title, nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSetWindowUserPointer(m_window, this);
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    glfwSetKeyCallback(m_window, [](GLFWwindow* win, int key, int, int action, int) {
        auto* self = static_cast<WindowsPlatform*>(glfwGetWindowUserPointer(win));
        if (self) self->OnKey(key, action);
    });
    glfwSetCursorPosCallback(m_window, [](GLFWwindow* win, double x, double y) {
        auto* self = static_cast<WindowsPlatform*>(glfwGetWindowUserPointer(win));
        if (self) self->OnCursor(x, y);
    });
    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* win, int button, int action, int) {
        auto* self = static_cast<WindowsPlatform*>(glfwGetWindowUserPointer(win));
        if (self) self->OnMouseButton(button, action);
    });

    return true;
}

void WindowsPlatform::OnKey(int key, int action) {
    if (key < 0 || key >= INPUT_KEY_COUNT) {
        return;
    }
    if (action == GLFW_PRESS) {
        m_inputState.keys[key] = true;
    } else if (action == GLFW_RELEASE) {
        m_inputState.keys[key] = false;
    }
}

void WindowsPlatform::OnCursor(double x, double y) {
    m_accumMouseDX += static_cast<int32_t>(x - m_lastMouseX);
    m_accumMouseDY += static_cast<int32_t>(y - m_lastMouseY);
    m_lastMouseX = x;
    m_lastMouseY = y;
}

void WindowsPlatform::OnMouseButton(int button, int action) {
    if (button < 0 || button >= 5) {
        return;
    }
    if (action == GLFW_PRESS) {
        m_inputState.mouseButtons[button] = true;
    } else if (action == GLFW_RELEASE) {
        m_inputState.mouseButtons[button] = false;
    }
}

void WindowsPlatform::PollEvents() {
    glfwPollEvents();
    // Escape belongs to the game (it saves and returns to the menu); only the
    // window's close button ends the program.
    if (glfwWindowShouldClose(m_window)) {
        m_shouldQuit = true;
    }

    m_inputState.mouseDX = static_cast<int16_t>(m_accumMouseDX);
    m_inputState.mouseDY = static_cast<int16_t>(m_accumMouseDY);
    m_accumMouseDX = 0;
    m_accumMouseDY = 0;
}

void WindowsPlatform::SwapBuffers() {
    if (m_window) {
        glfwSwapBuffers(m_window);
    }
}

bool WindowsPlatform::ShouldQuit() const {
    return m_shouldQuit;
}

double WindowsPlatform::GetTime() const {
    return glfwGetTime();
}

InputState WindowsPlatform::GetInputState() const {
    return m_inputState;
}

void WindowsPlatform::SetWindowShouldCloseCallback(bool (*callback)(bool)) {
    (void)callback;
}

void WindowsPlatform::SetCloseCallback(CloseCallback cb) {
    m_closeCallback = cb;
}

void WindowsPlatform::SetWindowTitle(const char* title) {
    if (m_window) {
        glfwSetWindowTitle(m_window, title);
    }
}

void WindowsPlatform::SetCursorCaptured(bool captured) {
    if (!m_window) {
        return;
    }
    glfwSetInputMode(m_window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, captured ? GLFW_TRUE : GLFW_FALSE);
    }
    // Drop the movement accumulated while the cursor was free so the camera does
    // not snap when gameplay starts.
    m_accumMouseDX = 0;
    m_accumMouseDY = 0;
    double cursorX = 0.0;
    double cursorY = 0.0;
    glfwGetCursorPos(m_window, &cursorX, &cursorY);
    m_lastMouseX = cursorX;
    m_lastMouseY = cursorY;
    m_inputState.mouseDX = 0;
    m_inputState.mouseDY = 0;
}

void WindowsPlatform::ShutdownWindow() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

const char* WindowsPlatform::GetSaveDir() const {
    static std::string saveDir;
    if (saveDir.empty()) {
        char appdata[MAX_PATH] = { 0 };
        if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appdata))) {
            saveDir = std::string(appdata) + "\\BlockForge";
        } else {
            saveDir = ".blockforge";
        }
        std::error_code ec;
        fs::create_directories(saveDir, ec);
    }
    return saveDir.c_str();
}

void WindowsPlatform::SetWindowIcon(const uint8_t* pixels, uint32_t width, uint32_t height) {
    (void)pixels; (void)width; (void)height;
}
