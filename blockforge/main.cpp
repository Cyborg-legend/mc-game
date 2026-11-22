#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>

#include "platform/common/iplatform.h"
#include "renderer/opengl/opengl_renderer.h"
#include "core/ecs/world.h"
#include "core/physics/player.h"
#include "core/entities/player_entity.h"
#include "core/entities/mob.h"
#include "audio/audio.h"
#include "debug/debug_overlay.h"

#include <vector>
#include <string>

static bool s_running = true;
static uint32_t s_frame_count = 0;
static double s_last_time = 0.0;

int main() {
    // Initialize platform
    IPlatform* platform = CreatePlatform();
    if (!platform->InitWindow(1280, 720, "BlockForge")) {
        std::cerr << "Failed to create window!" << std::endl;
        return -1;
    }

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW!" << std::endl;
        return -1;
    }

    // Set up input
    platform->SetWindowShouldCloseCallback([](bool) {});

    // Initialize audio
    AudioSystem::Initialize();

    // Initialize debug overlay
    DebugOverlay::Initialize();

    // Create world
    World world;
    world.GenerateChunks();

    // Create player
    Player player;
    player.position = { 0.0f, 20.0f, 0.0f };
    player.onGround = true;

    // Create player entity
    PlayerEntity playerEntity;
    playerEntity.entityId = 0;
    playerEntity.position = player.position;
    playerEntity.velocity = { 0.0f, 0.0f, 0.0f };
    playerEntity.onGround = true;
    playerEntity.health = 20;
    playerEntity.input = PlayerInput{};

    // Create OpenGL renderer
    OpenGLRenderer renderer;
    renderer.Initialize(1280, 720);

    // Main loop - fixed 20Hz physics
    const float physics_dt = 1.0f / 20.0f;
    float accumulator = 0.0f;
    double last_time = platform->GetTime();

    while (!platform->ShouldQuit()) {
        double current_time = platform->GetTime();
        float frame_time = (float)(current_time - last_time);
        last_time = current_time;
        accumulator += frame_time;

        // Poll events
        platform->PollEvents();

        // Input handling
        InputState input = platform->GetInputState();

        // Fixed timestep physics
        while (accumulator >= physics_dt) {
            // Update player physics
            player.Update(physics_dt, input);
            playerEntity.Update(physics_dt, input);

            // Update mobs
            for (auto& mob : world.GetMobs()) {
                mob.Update(world, physics_dt);
            }

            // Check block interactions
            playerEntity.HandleBlockInteraction(world, input);

            accumulator -= physics_dt;
        }

        // Render
        renderer.BeginFrame();
        world.Render(renderer, player);
        renderer.EndFrame();

        // Update debug overlay
        DebugOverlay::UpdateStats((float)frame_time, renderer.GetDrawCalls(), world.GetChunkCount(), world.GetEntityCount());

        s_frame_count++;
    }

    // Save on quit
    world.Save();
    AudioSystem::Shutdown();
    DebugOverlay::Shutdown();
    platform->ShutdownWindow();

    delete platform;
    return 0;
}