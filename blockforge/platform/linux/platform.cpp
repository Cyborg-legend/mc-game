#include "platform/linux/platform.h"
#include "platform/common/iplatform.h"

#include <GL/glew.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

LinuxPlatform::LinuxPlatform() = default;
LinuxPlatform::~LinuxPlatform() = default;

bool LinuxPlatform::InitWindow(uint32_t width, uint32_t height, const char* title) {
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
        auto* self = static_cast<LinuxPlatform*>(glfwGetWindowUserPointer(win));
        if (self) self->OnKey(key, action);
    });
    glfwSetCursorPosCallback(m_window, [](GLFWwindow* win, double x, double y) {
        auto* self = static_cast<LinuxPlatform*>(glfwGetWindowUserPointer(win));
        if (self) self->OnCursor(x, y);
    });
    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* win, int button, int action, int) {
        auto* self = static_cast<LinuxPlatform*>(glfwGetWindowUserPointer(win));
        if (self) self->OnMouseButton(button, action);
    });

    return true;
}

void LinuxPlatform::OnKey(int key, int action) {
    if (key < 0 || key >= 256) {
        return;
    }
    if (action == GLFW_PRESS) {
        m_inputState.keys[key] = true;
    } else if (action == GLFW_RELEASE) {
        m_inputState.keys[key] = false;
    }
}

void LinuxPlatform::OnCursor(double x, double y) {
    m_accumMouseDX += static_cast<int32_t>(x - m_lastMouseX);
    m_accumMouseDY += static_cast<int32_t>(y - m_lastMouseY);
    m_lastMouseX = x;
    m_lastMouseY = y;
}

void LinuxPlatform::OnMouseButton(int button, int action) {
    if (button < 0 || button >= 5) {
        return;
    }
    if (action == GLFW_PRESS) {
        m_inputState.mouseButtons[button] = true;
    } else if (action == GLFW_RELEASE) {
        m_inputState.mouseButtons[button] = false;
    }
}

void LinuxPlatform::PollEvents() {
    glfwPollEvents();
    if (glfwWindowShouldClose(m_window) || m_inputState.keys[GLFW_KEY_ESCAPE]) {
        m_shouldQuit = true;
    }

    m_inputState.mouseDX = static_cast<int16_t>(m_accumMouseDX);
    m_inputState.mouseDY = static_cast<int16_t>(m_accumMouseDY);
    m_accumMouseDX = 0;
    m_accumMouseDY = 0;
}

void LinuxPlatform::SwapBuffers() {
    if (m_window) {
        glfwSwapBuffers(m_window);
    }
}

bool LinuxPlatform::ShouldQuit() const {
    return m_shouldQuit;
}

double LinuxPlatform::GetTime() const {
    return glfwGetTime();
}

InputState LinuxPlatform::GetInputState() const {
    return m_inputState;
}

void LinuxPlatform::SetWindowShouldCloseCallback(bool (*callback)(bool)) {
    (void)callback;
}

void LinuxPlatform::SetCloseCallback(CloseCallback cb) {
    m_closeCallback = cb;
}

void LinuxPlatform::SetWindowTitle(const char* title) {
    if (m_window) {
        glfwSetWindowTitle(m_window, title);
    }
}

void LinuxPlatform::ShutdownWindow() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

const char* LinuxPlatform::GetSaveDir() const {
    static std::string saveDir;
    if (saveDir.empty()) {
        const char* home = std::getenv("HOME");
        saveDir = home ? (std::string(home) + "/.blockforge") : std::string(".blockforge");
        std::error_code ec;
        fs::create_directories(saveDir, ec);
    }
    return saveDir.c_str();
}

void LinuxPlatform::SetWindowIcon(const uint8_t* pixels, uint32_t width, uint32_t height) {
    (void)pixels; (void)width; (void)height;
}
