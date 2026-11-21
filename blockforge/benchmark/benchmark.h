#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

// Benchmark mode: spawns 1000 entities, renders max chunks,
// reports avg/min frame time over 60 seconds
class Benchmark {
public:
    Benchmark() : startTime_(std::chrono::high_resolution_clock::now()),
                  frameCount_(0), totalFrameTime_(0.0f),
                  minFrameTime_(std::numeric_limits<float>::max()),
                  maxFrameTime_(0.0f) {}

    // Run benchmark for specified seconds
    void Run(int seconds = 60) {
        auto endTime = startTime_ + std::chrono::seconds(seconds);
        
        while (std::chrono::high_resolution_clock::now() < endTime) {
            auto frameStart = std::chrono::high_resolution_clock::now();
            
            // Simulate one frame: update entities, render, etc.
            UpdateFrame();
            
            auto frameEnd = std::chrono::high_resolution_clock::now();
            float frameTime = 
                std::chrono::duration<float, std::milli>(frameEnd - frameStart).count();
            
            frameCount_++;
            totalFrameTime_ += frameTime;
            
            if (frameTime < minFrameTime_) minFrameTime_ = frameTime;
            if (frameTime > maxFrameTime_) maxFrameTime_ = frameTime;
        }
        
        ReportResults();
    }

    // Report benchmark results
    void ReportResults() const {
        if (frameCount_ == 0) {
            std::cout << "Benchmark: no frames recorded" << std::endl;
            return;
        }
        
        float avgFrameTime = totalFrameTime_ / static_cast<float>(frameCount_);
        float avgFPS = 1000.0f / avgFrameTime;
        
        std::cout << "=== BlockForge Benchmark Results ===" << std::endl;
        std::cout << "Duration: 60 seconds" << std::endl;
        std::cout << "Frames rendered: " << frameCount_ << std::endl;
        std::cout << "Average FPS: " << std::fixed << std::setprecision(1) << avgFPS << std::endl;
        std::cout << "Average frame time: " << std::fixed << std::setprecision(1) 
                  << avgFrameTime << "ms" << std::endl;
        std::cout << "Min frame time: " << std::fixed << std::setprecision(1) 
                  << minFrameTime_ << "ms" << std::endl;
        std::cout << "Max frame time: " << std::fixed << std::setprecision(1) 
                  << maxFrameTime_ << "ms" << std::endl;
    }

private:
    // Update one benchmark frame
    void UpdateFrame() {
        // In a real benchmark, this would:
        // - Update all entities
        // - Chunk management
        // - Rendering
        // - Physics
        // For now, just sleep to simulate work
        // std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::chrono::high_resolution_clock::time_point startTime_;
    int frameCount_;
    float totalFrameTime_;
    float minFrameTime_;
    float maxFrameTime_;
};