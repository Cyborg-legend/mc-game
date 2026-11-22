#include "platform/windows/platform.h"
#include "platform/common/iplatform.h"
#include <GL/glew.h>
#include <iostream>
#include <cstring>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

WindowsPlatform::WindowsPlatform() = default;
WindowsPlatform::~WindowsPlatform() = default;

bool WindowsPlatform::InitWindow(uint32_t width, uint32_t height, const char* title) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    
    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW!" << std::endl;
        return false;
    }

    glfwSetWindowTitle(m_window, title);
    SetWindowTitle(title);

    // Set up input callbacks
    glfwSetKeyCallback(m_window, [](GLFWwindow* win, int key, int scancode, int action, int mods) {
        auto* platform = static_cast<WindowsPlatform*>(glfwGetWindowUserPointer(win));
        if (key >= 0 && key < 256) {
            if (action == GLFW_PRESS) platform->m_inputState.keys[key] = true;
            else if (action == GLFW_RELEASE) platform->m_inputState.keys[key] = false;
        }
    });

    glfwSetCursorPosCallback(m_window, [](GLFWwindow* win, double xpos, double ypos) {
        auto* platform = static_cast<WindowsPlatform*>(glfwGetWindowUserPointer(win));
        platform->m_inputState.mouseDX = (int16_t)xpos;
        platform->m_inputState.mouseDY = (int16_t)ypos;
    });

    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* win, int button, int action, int mods) {
        auto* platform = static_cast<WindowsPlatform*>(glfwGetWindowUserPointer(win));
        if (button >= 0 && button < 5) {
            if (action == GLFW_PRESS) platform->m_inputState.mouseButtons[button] = true;
            else if (action == GLFW_RELEASE) platform->m_inputState.mouseButtons[button] = false;
        }
    });

    glfwSetWindowUserPointer(m_window, this);

    // Set up pointer lock
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    return true;
}

void WindowsPlatform::PollEvents() {
    glfwPollEvents();
    // Check if window should close
    if (glfwWindowShouldClose(m_window)) {
        m_shouldQuit = true;
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
    m_closeCallback = callback;
}

void WindowsPlatform::SetWindowTitle(const char* title) {
    if (m_window) {
        glfwSetWindowTitle(m_window, title);
    }
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
        // Get APPDATA on Windows
        char appdata[1024];
        if (SUCCEEDED(SHGetFolderA(NULL, CSIDL_APPDATA, NULL, 0, appdata))) {
            saveDir = std::string(appdata) + "\\blockforge";
        } else {
            saveDir = ".blockforge";
        }
        // Create directory
        fs::create_directories(saveDir);
    }
    return saveDir.c_str();
}

void WindowsPlatform::SetWindowIcon(const uint8_t* pixels, uint32_t width, uint32_t height) {
    // Not implemented for now
    (void)pixels; (void)width; (void)height;
}