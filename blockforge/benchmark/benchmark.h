#pragma once

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

// Benchmark mode: reports avg/min/max frame time over a fixed duration.
class Benchmark {
public:
    Benchmark()
        : startTime_(std::chrono::high_resolution_clock::now()),
          frameCount_(0),
          totalFrameTime_(0.0f),
          minFrameTime_(std::numeric_limits<float>::max()),
          maxFrameTime_(0.0f) {}

    void Run(int seconds = 60) {
        const auto endTime = startTime_ + std::chrono::seconds(seconds);
        while (std::chrono::high_resolution_clock::now() < endTime) {
            const auto frameStart = std::chrono::high_resolution_clock::now();
            UpdateFrame();
            const auto frameEnd = std::chrono::high_resolution_clock::now();
            const float frameTime =
                std::chrono::duration<float, std::milli>(frameEnd - frameStart).count();

            ++frameCount_;
            totalFrameTime_ += frameTime;
            if (frameTime < minFrameTime_) minFrameTime_ = frameTime;
            if (frameTime > maxFrameTime_) maxFrameTime_ = frameTime;
        }
        ReportResults();
    }

    void ReportResults() const {
        if (frameCount_ == 0) {
            std::cout << "Benchmark: no frames recorded" << std::endl;
            return;
        }
        const float avgFrameTime = totalFrameTime_ / static_cast<float>(frameCount_);
        const float avgFPS = 1000.0f / avgFrameTime;

        std::cout << "=== BlockForge Benchmark Results ===" << std::endl;
        std::cout << "Frames rendered: " << frameCount_ << std::endl;
        std::cout << std::fixed << std::setprecision(1);
        std::cout << "Average FPS: " << avgFPS << std::endl;
        std::cout << "Average frame time: " << avgFrameTime << "ms" << std::endl;
        std::cout << "Min frame time: " << minFrameTime_ << "ms" << std::endl;
        std::cout << "Max frame time: " << maxFrameTime_ << "ms" << std::endl;
    }

private:
    void UpdateFrame() {}

    std::chrono::high_resolution_clock::time_point startTime_;
    int frameCount_;
    float totalFrameTime_;
    float minFrameTime_;
    float maxFrameTime_;
};
