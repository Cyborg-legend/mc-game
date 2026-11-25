#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "audio/audio.h"
#include "core/entities/mob.h"
#include "core/log.h"
#include "core/physics/player.h"
#include "core/save/save.h"
#include "core/world/chunk.h"
#include "debug/debug_overlay.h"
#include "platform/common/iplatform.h"
#include "renderer/meshing/greedy_mesher.h"
#include "renderer/opengl/opengl_renderer.h"
#include "ui/menu.h"
#include "ui/ui.h"

namespace {

constexpr int kGateX = 12; // rift gate column used by both realms
constexpr int kGateZ = 8;
constexpr float kPhysicsDt = 1.0f / 20.0f;

int FloorDiv(int value, int divisor) {
    return (value >= 0) ? (value / divisor) : ((value - divisor + 1) / divisor);
}

// Rebuild the chunk holding a block plus the six chunks sharing a face with it
// (their border faces depend on the edited block).
void RemeshBlock(World& world, OpenGLRenderer& renderer, int worldX, int worldY, int worldZ) {
    static const int kOffsets[7][3] = {
        {0, 0, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    };
    const int baseCX = FloorDiv(worldX, CHUNK_SIZE);
    const int baseCY = FloorDiv(worldY, CHUNK_SIZE);
    const int baseCZ = FloorDiv(worldZ, CHUNK_SIZE);

    for (const auto& offset : kOffsets) {
        const uint64_t key = World::PackKey(baseCX + offset[0], baseCY + offset[1], baseCZ + offset[2]);
        auto it = world.GetChunks().find(key);
        if (it != world.GetChunks().end()) {
            renderer.uploadChunk(key, GreedyMesher::Mesh(world, it->second));
        }
    }
}

// Builds a small rift gate: voidstone frame, portal surface and glowing corner
// stones. Returns the floor height the player should stand on.
int BuildRiftGate(World& world, int gateX, int gateZ) {
    int baseY = world.SurfaceY(gateX, gateZ);
    if (baseY < 3) baseY = 3;
    const int y = baseY;

    // Carve a pocket so the gate is always reachable.
    for (int dy = -1; dy <= 5; ++dy) {
        for (int dz = -3; dz <= 3; ++dz) {
            for (int dx = -3; dx <= 4; ++dx) {
                if (world.GetBlock(gateX + dx, y + dy, gateZ + dz) == BlockType::Bedrock) {
                    continue;
                }
                world.SetBlock(gateX + dx, y + dy, gateZ + dz, BlockType::Air);
            }
        }
    }

    // Solid floor under the pocket.
    for (int dz = -3; dz <= 3; ++dz) {
        for (int dx = -3; dx <= 4; ++dx) {
            world.SetBlock(gateX + dx, y - 1, gateZ + dz, BlockType::Voidstone);
        }
    }

    // Frame.
    for (int dy = -1; dy <= 3; ++dy) {
        world.SetBlock(gateX - 1, y + dy, gateZ, BlockType::Voidstone);
        world.SetBlock(gateX + 2, y + dy, gateZ, BlockType::Voidstone);
    }
    for (int dx = -1; dx <= 2; ++dx) {
        world.SetBlock(gateX + dx, y - 1, gateZ, BlockType::Voidstone);
        world.SetBlock(gateX + dx, y + 3, gateZ, BlockType::Voidstone);
    }

    // Rift surface (2 wide, 3 tall).
    for (int dy = 0; dy < 3; ++dy) {
        world.SetBlock(gateX, y + dy, gateZ, BlockType::Portal);
        world.SetBlock(gateX + 1, y + dy, gateZ, BlockType::Portal);
    }

    // Glowing corner stones make the gate readable in the dark realm.
    world.SetBlock(gateX - 1, y + 2, gateZ, BlockType::Lumenstone);
    world.SetBlock(gateX + 2, y + 2, gateZ, BlockType::Lumenstone);

    return y;
}

bool PlayerInsideRift(const World& world, const Player& player) {
    const int bx = static_cast<int>(std::floor(player.x));
    const int bz = static_cast<int>(std::floor(player.z));
    const int feetY = static_cast<int>(std::floor(player.y));
    return world.GetBlock(bx, feetY, bz) == BlockType::Portal ||
           world.GetBlock(bx, feetY + 1, bz) == BlockType::Portal;
}

enum class AppState {
    Menu,
    Playing
};

} // namespace

int main() {
    IPlatform* platform = CreatePlatform();
    if (!platform) {
        std::fprintf(stderr, "Failed to create platform backend\n");
        return 1;
    }

    const std::string saveDir = platform->GetSaveDir();
    Log::Init(saveDir + "/blockforge.log");
    Log::Write("BlockForge starting");

    if (!platform->InitWindow(1280, 720, "BlockForge")) {
        Log::ShowFatal("BlockForge could not open an OpenGL window.\n\nDetails: " + Log::Path());
        delete platform;
        return 1;
    }

    OpenGLRenderer renderer;
    if (!renderer.init()) {
        Log::ShowFatal("BlockForge could not start its OpenGL renderer.\n\nDetails: " + Log::Path());
        platform->ShutdownWindow();
        delete platform;
        return 1;
    }
    Log::Write("Renderer ready");

    AudioSystem audio;
    if (!audio.Initialize()) {
        Log::Write("Audio unavailable, continuing without sound");
    }

    DebugOverlay::Initialize();

    const std::string savePath = saveDir + "/world.sav";
    bool hasSave = std::filesystem::exists(savePath);

    MainMenu menu;
    menu.Reset(hasSave);

    World world;
    std::vector<uint64_t> visibleChunks;
    visibleChunks.reserve(4096);

    Player player;
    std::vector<Mob> mobs;
    float portalCooldown = 0.0f;
    int chunkRadius = menu.Options().renderDistance;

    Camera camera;
    camera.aspect = 1280.0f / 720.0f;

    UiDrawList ui;

    AppState state = AppState::Menu;
    platform->SetCursorCaptured(false);

    auto uploadWorld = [&]() {
        renderer.clearChunks();
        visibleChunks.clear();
        visibleChunks.reserve(world.GetChunkCount());
        for (const auto& entry : world.GetChunks()) {
            renderer.uploadChunk(entry.first, GreedyMesher::Mesh(world, entry.second));
            visibleChunks.push_back(entry.first);
        }
    };

    auto spawnMobs = [&]() {
        mobs.clear();
        const MobType type = (world.GetDimension() == Dimension::Underworld)
                                 ? MobType::Burner
                                 : MobType::Wanderer;
        for (int i = 0; i < 24; ++i) {
            const int mx = (i % 6) * 16 - 40;
            const int mz = (i / 6) * 16 - 40;
            Mob mob;
            mob.x = static_cast<float>(kGateX + mx) + 0.5f;
            mob.z = static_cast<float>(kGateZ + mz) + 0.5f;
            mob.y = static_cast<float>(world.SurfaceY(static_cast<int>(mob.x), static_cast<int>(mob.z)));
            mob.type = type;
            mobs.push_back(mob);
        }
    };

    auto applyEnvironment = [&]() {
        if (world.GetDimension() == Dimension::Underworld) {
            renderer.setEnvironment(OpenGLRenderer::UnderworldEnvironment());
        } else {
            renderer.setEnvironment(OpenGLRenderer::UpperworldEnvironment());
        }
    };

    auto updateTitle = [&]() {
        platform->SetWindowTitle(world.GetDimension() == Dimension::Underworld
                                     ? "BlockForge | Underworld"
                                     : "BlockForge | Upperworld");
    };

    auto placePlayerAtGate = [&](int gateY) {
        player = Player();
        player.x = static_cast<float>(kGateX - 3) + 0.5f;
        player.z = static_cast<float>(kGateZ) + 0.5f;
        player.y = static_cast<float>(gateY) + 1.0f;
        player.yaw = 0.0f; // face the rift gate
        player.vx = 0.0f;
        player.vy = 0.0f;
        player.vz = 0.0f;
        player.onGround = false;

        camera.position = glm::vec3(player.x, player.y + 1.62f, player.z);
        camera.yaw = player.yaw;
        camera.pitch = player.pitch;
        portalCooldown = 2.0f;
    };

    auto saveWorld = [&]() {
        if (world.GetChunkCount() == 0) {
            return;
        }
        SaveSlot slot;
        slot.seed = world.GetSeed();
        slot.world = world;
        for (int i = 0; i < 36; ++i) {
            slot.inventory[static_cast<size_t>(i)] = player.inventory[i];
        }
        if (!slot.SaveToFile(savePath)) {
            Log::Write("Failed to save world: " + savePath);
            return;
        }
        GameSettings settings;
        settings.renderDistance = chunkRadius;
        SaveSettings(saveDir + "/settings.json", settings);
        hasSave = true;
        Log::Write("Saved world to " + savePath);
    };

    auto goToMenu = [&]() {
        saveWorld();
        menu.Reset(hasSave);
        state = AppState::Menu;
        platform->SetCursorCaptured(false);
        platform->SetWindowTitle("BlockForge");
    };

    auto travel = [&](Dimension target) {
        world.SetDimension(target);
        world.Clear();
        world.GenerateChunksAround(FloorDiv(kGateX, CHUNK_SIZE), FloorDiv(kGateZ, CHUNK_SIZE), chunkRadius);
        const int gateY = BuildRiftGate(world, kGateX, kGateZ);
        uploadWorld();

        // Arrive inside the gate alcove, three blocks in front of the portal.
        player.x = static_cast<float>(kGateX - 3) + 0.5f;
        player.z = static_cast<float>(kGateZ) + 0.5f;
        player.y = static_cast<float>(gateY) + 1.0f;
        player.vx = 0.0f;
        player.vy = 0.0f;
        player.vz = 0.0f;
        player.onGround = false;

        portalCooldown = 2.0f;
        spawnMobs();
        applyEnvironment();
        updateTitle();
        audio.PlaySound("portal", player.x, player.y, player.z, 0.9f);
    };

    auto startNewWorld = [&](const WorldOptions& options) {
        world = World();
        world.SetSeed(options.seed);
        world.SetDimension(options.startingRealm);
        chunkRadius = options.renderDistance;
        Log::Write("Generating world seed=" + std::to_string(options.seed) +
                   " radius=" + std::to_string(options.renderDistance) +
                   " realm=" + DimensionName(options.startingRealm));

        world.GenerateChunksAround(FloorDiv(kGateX, CHUNK_SIZE), FloorDiv(kGateZ, CHUNK_SIZE), chunkRadius);
        const int gateY = BuildRiftGate(world, kGateX, kGateZ);
        uploadWorld();
        placePlayerAtGate(gateY);
        spawnMobs();
        applyEnvironment();
        updateTitle();
        audio.PlaySound("portal", player.x, player.y, player.z, 0.9f);
        state = AppState::Playing;
    };

    auto continueWorld = [&]() {
        SaveSlot slot;
        if (!slot.LoadFromFile(savePath)) {
            Log::Write("Could not load " + savePath);
            hasSave = false;
            menu.Reset(hasSave);
            return;
        }
        world = slot.world;
        chunkRadius = menu.Options().renderDistance;
        Log::Write("Loaded world seed=" + std::to_string(slot.seed) +
                   " realm=" + DimensionName(world.GetDimension()) +
                   " chunks=" + std::to_string(world.GetChunkCount()));

        const int gateY = BuildRiftGate(world, kGateX, kGateZ);
        uploadWorld();
        placePlayerAtGate(gateY);
        for (size_t i = 0; i < slot.inventory.size() && i < 36; ++i) {
            player.inventory[i] = slot.inventory[i];
        }
        spawnMobs();
        applyEnvironment();
        updateTitle();
        state = AppState::Playing;
    };

    auto drawLoadingScreen = [&](const std::string& message) {
        int viewportW = 1280;
        int viewportH = 720;
        renderer.viewportSize(viewportW, viewportH);
        renderer.render(camera, std::vector<uint64_t>());

        ui.Clear();
        ui.Rect(0.0f, 0.0f, static_cast<float>(viewportW), static_cast<float>(viewportH),
                { 0.05f, 0.06f, 0.09f, 1.0f });
        ui.TextCentered(static_cast<float>(viewportW) * 0.5f,
                        static_cast<float>(viewportH) * 0.44f, 3.0f, message,
                        { 0.98f, 0.86f, 0.52f, 1.0f });
        ui.TextCentered(static_cast<float>(viewportW) * 0.5f,
                        static_cast<float>(viewportH) * 0.58f, 1.6f,
                        "CARVING THE REALMS", { 0.62f, 0.70f, 0.85f, 1.0f });
        renderer.renderUiVertices(ui.Vertices());
        platform->SwapBuffers();
        platform->PollEvents();
    };

    float accumulator = 0.0f;
    double lastTime = platform->GetTime();

    while (!platform->ShouldQuit()) {
        const double now = platform->GetTime();
        float frameTime = static_cast<float>(now - lastTime);
        lastTime = now;
        if (frameTime > 0.25f) {
            frameTime = 0.25f;
        }
        accumulator += frameTime;

        platform->PollEvents();
        const InputState input = platform->GetInputState();

        if (state == AppState::Menu) {
            const MenuAction action = menu.Update(input, hasSave);

            if (action == MenuAction::QuitGame) {
                break;
            }
            if (action == MenuAction::NewGame) {
                drawLoadingScreen("GENERATING WORLD");
                startNewWorld(menu.Options());
                platform->SetCursorCaptured(true);
            } else if (action == MenuAction::ContinueGame) {
                drawLoadingScreen("LOADING WORLD");
                continueWorld();
                if (state == AppState::Playing) {
                    platform->SetCursorCaptured(true);
                }
            }

            if (state == AppState::Menu) {
                int viewportW = 1280;
                int viewportH = 720;
                renderer.viewportSize(viewportW, viewportH);
                camera.aspect = static_cast<float>(viewportW) / static_cast<float>(viewportH > 0 ? viewportH : 1);
                renderer.render(camera, std::vector<uint64_t>());
                ui.Clear();
                menu.Draw(ui, static_cast<float>(viewportW), static_cast<float>(viewportH), hasSave);
                renderer.renderUiVertices(ui.Vertices());
                platform->SwapBuffers();
            }

            DebugOverlay::UpdateStats(frameTime, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, "Menu");
            continue;
        }

        // ---- Playing -------------------------------------------------------
        if (input.keys[GLFW_KEY_ESCAPE]) {
            Log::Write("Returning to the menu");
            goToMenu();
            continue;
        }

        portalCooldown = (portalCooldown > frameTime) ? (portalCooldown - frameTime) : 0.0f;

        // Mouse look.
        camera.Look(static_cast<float>(input.mouseDX), static_cast<float>(input.mouseDY));

        // Hotbar selection (1-9).
        for (int slot = 0; slot < 9; ++slot) {
            if (input.keys[GLFW_KEY_1 + slot]) {
                player.selectedSlot = static_cast<int8_t>(slot);
            }
        }

        player.yaw = camera.yaw;
        player.pitch = camera.pitch;

        const bool jumpPressed = input.keys[GLFW_KEY_SPACE] && player.onGround;

        while (accumulator >= kPhysicsDt) {
            player.Update(kPhysicsDt, input, world);
            for (Mob& mob : mobs) {
                mob.Update(kPhysicsDt, world);
            }
            accumulator -= kPhysicsDt;
        }

        if (jumpPressed) {
            audio.PlaySound("jump", player.x, player.y, player.z, 0.5f);
        }

        // Block interaction.
        if (input.mouseButtons[0]) {
            int bx = 0, by = 0, bz = 0;
            BlockType type = BlockType::Air;
            if (player.Raycast(world, 6.0f, bx, by, bz, type) && IsBreakable(type)) {
                world.SetBlock(bx, by, bz, BlockType::Air);
                RemeshBlock(world, renderer, bx, by, bz);
                audio.PlaySound("break", static_cast<float>(bx), static_cast<float>(by), static_cast<float>(bz), 0.7f);
            }
        }
        if (input.mouseButtons[1]) {
            player.selectedSlot = (player.selectedSlot < 0) ? 0 : (player.selectedSlot % 9);
            const BlockType place = static_cast<BlockType>(player.hotbar[player.selectedSlot]);
            int px = 0, py = 0, pz = 0;
            if (place != BlockType::Air && player.PlaceBlock(world, place, &px, &py, &pz)) {
                audio.PlaySound("place", player.x, player.y, player.z, 0.6f);
                RemeshBlock(world, renderer, px, py, pz);
            }
        }

        // Rift travel between the two realms.
        if (portalCooldown <= 0.0f && PlayerInsideRift(world, player)) {
            const Dimension next = (world.GetDimension() == Dimension::Underworld)
                                       ? Dimension::Upperworld
                                       : Dimension::Underworld;
            travel(next);
        }

        // Camera follows the player's eye.
        camera.position = glm::vec3(player.x, player.y + 1.62f, player.z);
        audio.UpdateListener(camera.position.x, camera.position.y, camera.position.z);

        int viewportW = 1280;
        int viewportH = 720;
        renderer.viewportSize(viewportW, viewportH);
        camera.aspect = static_cast<float>(viewportW) / static_cast<float>(viewportH > 0 ? viewportH : 1);

        renderer.render(camera, visibleChunks);

        HudState hud;
        hud.selectedSlot = player.selectedSlot;
        for (int i = 0; i < 9; ++i) {
            hud.hotbar[i] = player.hotbar[i];
        }
        hud.health = static_cast<int>(player.health);
        hud.maxHealth = 20;
        renderer.renderHud(hud);

        platform->SwapBuffers();

        DebugOverlay::UpdateStats(frameTime,
                                  static_cast<uint32_t>(renderer.getDrawCalls()),
                                  static_cast<uint32_t>(world.GetChunkCount()),
                                  static_cast<uint32_t>(mobs.size()),
                                  player.x, player.y, player.z,
                                  player.yaw, player.pitch,
                                  DimensionName(world.GetDimension()));
        DebugOverlay::Render();
    }

    if (state == AppState::Playing) {
        saveWorld();
    }

    audio.Shutdown();
    renderer.shutdown();
    platform->ShutdownWindow();
    delete platform;
    Log::Write("BlockForge closed");
    return 0;
}
