#include "iplatform.h"
#include <GLFW/glfw3.h>
#include <fstream>
#include <iostream>
#include <string>

namespace platform::windows {

bool Platform::InitWindow(const char* title, int width, int height) {
    (void)width;
    (void)height;

    // GLFW will create the window; we just need to initialize it if not already
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    glfwWindow_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!glfwWindow_) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    // Set user pointer for event handling
    glfwSetWindowUserPointer(glfwWindow_, this);

    // Window close callback
    glfwSetWindowCloseCallback(glfwWindow_, [](GLFWwindow* window) {
        auto* self = static_cast<Platform*>(glfwGetWindowUserPointer(window));
        if (self) self->quit_ = true;
    });

    // Focus callback
    glfwSetWindowFocusCallback(glfwWindow_, [](GLFWwindow* window, int focused) {
        auto* self = static_cast<Platform*>(glfwGetWindowUserPointer(window));
        if (self) {
            // Handle focus loss if needed
        }
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
    // Return the GLFW window as the Vulkan surface will be created from it
    // The actual VkSurfaceKHR is created by the Vulkan instance
    return glfwWindow_;
}

void* Platform::GetVulkanInstance() {
    return glfwGetVulkanInstance();
}

std::string Platform::GetSaveDir() {
    // Windows: %APPDATA%/BlockForge
    char* appdata = nullptr;
    _dupenv_s(&appdata, nullptr, "APPDATA");
    std::string result;
    if (appdata) {
        result = std::string(appdata) + "/BlockForge";
        Free(appdata);
    } else {
        result = ".blockforge";
    }
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
    if (glfwInit()) {  // Only terminate if initialized
        glfwTerminate();
    }
}

} // namespace platform::windows