#include "core/log.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

std::ofstream g_file;
std::string g_path;

std::string Timestamp() {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    char buffer[32] = { 0 };
    std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &tm);
    return buffer;
}

} // namespace

void Log::Init(const std::string& path) {
    g_path = path;
    g_file.open(path, std::ios::out | std::ios::trunc);
    if (!g_file.is_open()) {
        std::cerr << "Could not open log file: " << path << std::endl;
    }
}

void Log::Write(const std::string& message) {
    if (g_file.is_open()) {
        g_file << "[" << Timestamp() << "] " << message << std::endl;
        g_file.flush();
    }
    std::cerr << message << std::endl;
}

const std::string& Log::Path() {
    return g_path;
}

void Log::ShowFatal(const std::string& message) {
    Write(message);

#if defined(_WIN32)
    MessageBoxA(nullptr, message.c_str(), "BlockForge", MB_OK | MB_ICONERROR);
#else
    std::cerr << message << std::endl;
#endif
}
