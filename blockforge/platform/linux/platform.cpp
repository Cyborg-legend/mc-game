#include "iplatform.h"
#include <GLFW/glfw3.h>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/stat.h>

namespace platform::linux {

bool Platform::InitWindow(const char* title, int width, int height) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    // Prefer Wayland, fall back to X11 if unavailable
    #ifdef GLFW_WAYLANG_LIBRARY
    glfwWindowHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
    #endif

    glfwWindow_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!glfwWindow_) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        // Fallback to X11
        #ifdef GLFW_X11_LIBRARY
        glfwWindowHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
        glfwWindow_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
        #endif
        if (!glfwWindow_) {
            glfwTerminate();
            return false;
        }
    }

    glfwSetWindowUserPointer(glfwWindow_, this);

    glfwSetWindowCloseCallback(glfwWindow_, [](GLFWwindow* window) {
        auto* self = static_cast<Platform*>(glfwGetWindowUserPointer(window));
        if (self) self->quit_ = true;
    });

    return true;
}

void Platform::PollEvents() {
    glfwPollEvents();
}

bool Platform::ShouldQuit() const {
    return quit_;
}

void* Platform::GetVulkanSurface() {
    return glfwWindow_;
}

void* Platform::GetVulkanInstance() {
    return glfwGetVulkanInstance();
}

std::string Platform::GetSaveDir() {
    // Linux: ~/.config/BlockForge or ~/.local/share/BlockForge
    // Use XDG Base Directory spec
    const char* xdg = getenv("XDG_CONFIG_HOME");
    std::string result;
    if (xdg) {
        result = std::string(xdg) + "/BlockForge";
    } else {
        const char* home = getenv("HOME");
        if (home) {
            result = std::string(home) + "/.config/BlockForge";
        } else {
            result = ".blockforge";
        }
    }
    // Create directory
    mkdir(result.c_str(), 0755);
    return result;
}

void Platform::SetWindowTitle(const char* title) {
    if (glfwWindow_) {
        glfwSetWindowTitle(glfwWindow_, title);
    }
}

void Platform::Cleanup() {
    if (glfwWindow_) {
        glfwDestroyWindow(glfwWindow_);
        glfwWindow_ = nullptr;
    }
    // Don't terminate GLFW here - let the app manage it
}

} // namespace platform::linux