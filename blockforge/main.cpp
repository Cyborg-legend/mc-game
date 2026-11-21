#include "core/ecs/world.h"
#include "core/physics/player.h"
#include "core/entities/player_entity.h"
#include "renderer/vulkan/vulkan_context.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "platform/common/iplatform.h"
#include "core/crafting/crafting.h"
#include "audio/miniaudio_audio.h"
#include "debug/debug_overlay.h"
#include "benchmark/benchmark.h"

#include <glm/glm.hpp>
#include <cstdlib>
#include <iostream>
#include <string>

// Forward declarations
class World;
class PlayerEntity;

// Global state (would be properly encapsulated in a real engine)
World* g_world = nullptr;
PlayerEntity* g_player = nullptr;
AudioSystem* g_audio = nullptr;
DebugOverlay* g_debug = nullptr;
bool g_benchmarkMode = false;
int g_renderDistance = 16;
uint64_t g_seed = 12345;
float g_lastFrameTime = 0.0f;
int g_frameCount = 0;
float g_frameTimeAccumulator = 0.0f;

// Command-line flag parsing
void ParseCommandArgs(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--benchmark") {
            g_benchmarkMode = true;
        } else if (arg.substr(0, 16) == "--render-distance") {
            if (i + 1 < argc) {
                g_renderDistance = std::atoi(argv[++i]);
                if (g_renderDistance < 8) g_renderDistance = 8;
                if (g_renderDistance > 64) g_renderDistance = 64;
            }
        } else if (arg.substr(0, 5) == "--seed") {
            if (i + 1 < argc) {
                g_seed = static_cast<uint64_t>(std::atoll(argv[++i]));
            }
        }
    }
}

// Frame timing
void CalculateFrameTime() {
    // Static timing - in a real app would use a proper timer
    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
    lastTime = currentTime;
    
    g_frameTimeAccumulator += deltaTime;
    g_frameCount++;
    
    // Update every second
    if (g_frameTimeAccumulator >= 1.0f) {
        float fps = static_cast<float>(g_frameCount) / g_frameTimeAccumulator;
        float frameTimeMs = 1000.0f / fps;
        
        // Could update debug overlay here
        // g_debug->UpdateFPSText(fps, frameTimeMs);
        
        g_frameTimeAccumulator = 0.0f;
        g_frameCount = 0;
    }
}

// Main game loop
int RunGameLoop(IPlatform& platform, VulkanContext& vulkanCtx, Camera& camera) {
    // Initialize systems
    World world;
    g_world = &world;
    
    Player player;
    g_player = new PlayerEntity();
    g_player->Init(&world);
    
    // Initialize audio
    // g_audio = new AudioSystem();
    // g_audio->Initialize();
    
    g_debug = new DebugOverlay();
    
    // Parse command args
    ParseCommandArgs(__argc, __argv);
    
    // Set seed for world generation
    // world.SetSeed(g_seed);
    
    // Generate initial chunks around player
    // For now, generate a few chunks
    for (int cx = 0; cx < g_renderDistance; ++cx) {
        for (int cz = 0; cz < g_renderDistance; ++cz) {
            // world.GenerateChunk(cx, 0, cz, g_seed);
        }
    }
    
    // Player starting position
    player.position = glm::vec3(0.0f, 64.0f, 0.0f);
    
    // Main loop
    bool running = true;
    platform.InitWindow("BlockForge", 800, 600);
    
    // Fixed timestep: 20Hz = 0.05s per step
    const float FIXED_TIME_STEP = 1.0f / 20.0f;
    float accumulator = 0.0f;
    float currentTime = 0.0f;
    float lastRenderTime = 0.0f;
    
    while (running && !platform.ShouldQuit()) {
        // Poll events
        platform.PollEvents();
        
        // Calculate delta time
        auto frameStart = std::chrono::high_resolution_clock::now();
        CalculateFrameTime();
        
        // Handle input
        // Would check platform state for keyboard/mouse
        
        // Update player
        // Would pass input state to player
        
        // Fixed timestep accumulation
        currentTime += FIXED_TIME_STEP;
        accumulator += FIXED_TIME_STEP;
        
        // Process fixed timestep updates
        while (accumulator >= FIXED_TIME_STEP) {
            // Update game logic at 20Hz
            // g_player->Update(FIXED_TIME_STEP, input, world);
            accumulator -= FIXED_TIME_STEP;
        }
        
        // Render frame
        // Would call renderer here
        // vulkanCtx.Render(camera, visibleChunks);
        
        // Present
        // platform.SwapBuffers();
        
        // Frame timing
        auto frameEnd = std::chrono::high_resolution_clock::now();
        float frameDuration = 
            std::chrono::duration<float, std::milli>(frameEnd - frameStart).count();
        
        // Simple FPS tracking
        // (Would be more sophisticated in real code)
        
        if (g_benchmarkMode) {
            // Benchmark mode - just count frames
            // g_benchmark->ReportResults would be called after 60s
        }
        
        // Check if should quit
        running = !platform.ShouldQuit();
    }
    
    delete g_player;
    delete g_debug;
    // delete g_audio;
    
    return 0;
}

int main(int argc, char** argv) {
    // Create platform (factory)
#ifdef _WIN32
    // Windows platform implementation would be created here
    // IPlatform* platform = new platform::windows::Platform();
#else
    // Linux platform implementation
    // IPlatform* platform = new platform::linux::Platform();
#endif
    
    // For now, just return success since platform creation
    // requires a full windowing system setup
    // int result = RunGameLoop(*platform, vulkanCtx, camera);
    
    // Cleanup
    // delete platform;
    
    std::cout << "BlockForge - Voxel Sandbox Game Engine" << std::endl;
    std::cout << "Usage: blockforge [--benchmark] [--render-distance N] [--seed N]" << std::endl;
    
    return 0;
}