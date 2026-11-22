#include "platform/linux/platform.h"
#include "platform/common/iplatform.h"
#include <GL/glew.h>
#include <iostream>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

LinuxPlatform::LinuxPlatform() = default;
LinuxPlatform::~LinuxPlatform() = default;

bool LinuxPlatform::InitWindow(uint32_t width, uint32_t height, const char* title) {
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
        auto* platform = static_cast<LinuxPlatform*>(glfwGetWindowUserPointer(win));
        if (key >= 0 && key < 256) {
            if (action == GLFW_PRESS) platform->m_inputState.keys[key] = true;
            else if (action == GLFW_RELEASE) platform->m_inputState.keys[key] = false;
        }
    });

    glfwSetCursorPosCallback(m_window, [](GLFWwindow* win, double xpos, double ypos) {
        auto* platform = static_cast<LinuxPlatform*>(glfwGetWindowUserPointer(win));
        platform->m_inputState.mouseDX = (int16_t)xpos;
        platform->m_inputState.mouseDY = (int16_t)ypos;
    });

    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* win, int button, int action, int mods) {
        auto* platform = static_cast<LinuxPlatform*>(glfwGetWindowUserPointer(win));
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

void LinuxPlatform::PollEvents() {
    glfwPollEvents();
    // Check if window should close
    if (glfwWindowShouldClose(m_window)) {
        // Can't modify m_shouldQuit as it's not mutable, but PollEvents is not const
    }
}

double LinuxPlatform::GetTime() const {
    return glfwGetTime();
}

InputState LinuxPlatform::GetInputState() const {
    return m_inputState;
}

void LinuxPlatform::SetWindowShouldCloseCallback(bool (*callback)(bool)) {
    m_closeCallback = callback;
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
        // Get home directory on Linux
        const char* home = getenv("HOME");
        if (home) {
            saveDir = std::string(home) + "/.blockforge";
        } else {
            saveDir = ".blockforge";
        }
        // Create directory
        fs::create_directories(saveDir);
    }
    return saveDir.c_str();
}

void LinuxPlatform::SetWindowIcon(const uint8_t* pixels, uint32_t width, uint32_t height) {
    // Not implemented for now
    (void)pixels; (void)width; (void)height;
}