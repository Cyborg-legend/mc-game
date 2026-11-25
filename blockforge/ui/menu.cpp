#include "ui/menu.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdlib>

namespace {

const UiColor kBackdrop{ 0.05f, 0.06f, 0.09f, 1.0f };
const UiColor kBand{ 0.10f, 0.12f, 0.18f, 1.0f };
const UiColor kTitle{ 0.98f, 0.86f, 0.52f, 1.0f };
const UiColor kText{ 0.92f, 0.95f, 1.00f, 1.0f };
const UiColor kMuted{ 0.62f, 0.70f, 0.85f, 1.0f };
const UiColor kValue{ 0.72f, 0.78f, 0.88f, 1.0f };
const UiColor kValueActive{ 1.00f, 0.85f, 0.45f, 1.0f };
const UiColor kFillIdle{ 0.11f, 0.13f, 0.18f, 1.0f };
const UiColor kFillActive{ 0.20f, 0.33f, 0.52f, 1.0f };
const UiColor kBorderIdle{ 0.24f, 0.28f, 0.36f, 1.0f };
const UiColor kBorderActive{ 0.55f, 0.78f, 1.00f, 1.0f };

const char* RealmLabel(Dimension dimension) {
    return dimension == Dimension::Underworld ? "UNDERWORLD" : "UPPERWORLD";
}

} // namespace

bool KeyEdge::Pressed(const InputState& input, int key) const {
    if (key < 0 || key >= INPUT_KEY_COUNT) {
        return false;
    }
    return input.keys[key] && !previous[key];
}

void KeyEdge::Sync(const InputState& input) {
    for (int i = 0; i < INPUT_KEY_COUNT; ++i) {
        previous[i] = input.keys[i];
    }
}

void KeyEdge::Reset() {
    for (bool& key : previous) {
        key = false;
    }
}

void KeyEdge::Latch(int key) {
    if (key >= 0 && key < INPUT_KEY_COUNT) {
        previous[key] = true;
    }
}

MainMenu::MainMenu() {
    SyncSeedText();
}

void MainMenu::Reset(bool hasSave) {
    screen_ = Screen::Root;
    selection_ = hasSave ? 0 : 1;
    SyncSeedText();
    keys_.Reset();
}

void MainMenu::SyncSeedText() {
    seedText_ = std::to_string(options_.seed);
}

uint32_t MainMenu::ParseSeed() const {
    if (seedText_.empty()) {
        return 20251125u;
    }
    try {
        const unsigned long long value = std::stoull(seedText_);
        return static_cast<uint32_t>(value & 0xFFFFFFFFull);
    } catch (...) {
        return 20251125u;
    }
}

MenuAction MainMenu::Update(const InputState& input, bool hasSave) {
    MenuAction action = MenuAction::None;

    if (screen_ == Screen::Root) {
        if (keys_.Pressed(input, GLFW_KEY_UP)) {
            selection_ = (selection_ + 2) % 3;
        }
        if (keys_.Pressed(input, GLFW_KEY_DOWN)) {
            selection_ = (selection_ + 1) % 3;
        }
        if (keys_.Pressed(input, GLFW_KEY_ENTER) || keys_.Pressed(input, GLFW_KEY_KP_ENTER)) {
            if (selection_ == 0) {
                if (hasSave) {
                    action = MenuAction::ContinueGame;
                }
            } else if (selection_ == 1) {
                screen_ = Screen::Create;
                selection_ = 0;
                SyncSeedText();
            } else {
                action = MenuAction::QuitGame;
            }
        }
    } else {
        if (keys_.Pressed(input, GLFW_KEY_UP)) {
            selection_ = (selection_ + 4) % 5;
        }
        if (keys_.Pressed(input, GLFW_KEY_DOWN)) {
            selection_ = (selection_ + 1) % 5;
        }
        if (keys_.Pressed(input, GLFW_KEY_ESCAPE)) {
            screen_ = Screen::Root;
            selection_ = 1;
        }
        if (keys_.Pressed(input, GLFW_KEY_LEFT)) {
            if (selection_ == 1) {
                options_.renderDistance = std::max(3, options_.renderDistance - 1);
            } else if (selection_ == 2) {
                options_.startingRealm = Dimension::Upperworld;
            }
        }
        if (keys_.Pressed(input, GLFW_KEY_RIGHT)) {
            if (selection_ == 1) {
                options_.renderDistance = std::min(8, options_.renderDistance + 1);
            } else if (selection_ == 2) {
                options_.startingRealm = Dimension::Underworld;
            }
        }
        if (selection_ == 0) {
            for (int key = GLFW_KEY_0; key <= GLFW_KEY_9; ++key) {
                if (keys_.Pressed(input, key) && seedText_.size() < 10) {
                    seedText_.push_back(static_cast<char>('0' + (key - GLFW_KEY_0)));
                }
            }
            if (keys_.Pressed(input, GLFW_KEY_BACKSPACE) && !seedText_.empty()) {
                seedText_.pop_back();
            }
            if (keys_.Pressed(input, GLFW_KEY_R)) {
                options_.seed = static_cast<uint32_t>(std::rand()) * 2654435761u + 1013904223u;
                SyncSeedText();
            }
        }
        if (keys_.Pressed(input, GLFW_KEY_ENTER) || keys_.Pressed(input, GLFW_KEY_KP_ENTER)) {
            if (selection_ == 3) {
                options_.seed = ParseSeed();
                action = MenuAction::NewGame;
            } else if (selection_ == 4) {
                screen_ = Screen::Root;
                selection_ = 1;
            }
        }
    }

    keys_.Sync(input);
    return action;
}

namespace {

void DrawRow(UiDrawList& ui, float x, float y, float w, float h,
             const std::string& label, const std::string& value,
             bool selected) {
    const float scale = 2.0f;
    ui.Panel(x, y, w, h, selected ? kFillActive : kFillIdle,
             selected ? kBorderActive : kBorderIdle);
    const float textY = y + (h - 7.0f * scale) * 0.5f;
    ui.Text(x + 18.0f, textY, scale, label, kText);
    if (!value.empty()) {
        const float valueWidth = ui.TextWidth(value, scale);
        ui.Text(x + w - 18.0f - valueWidth, textY, scale, value,
                selected ? kValueActive : kValue);
    }
    if (selected) {
        // Left/right chevrons show the row can be changed with the arrow keys.
        const float chevronY = y + (h - 7.0f * scale) * 0.5f;
        ui.Text(x - 22.0f, chevronY, scale, "<", kValueActive);
        ui.Text(x + w + 8.0f, chevronY, scale, ">", kValueActive);
    }
}

void DrawButton(UiDrawList& ui, float x, float y, float w, float h,
                const std::string& label, bool selected, bool enabled) {
    const float scale = 2.0f;
    const UiColor fill = selected ? kFillActive : kFillIdle;
    const UiColor border = selected ? kBorderActive : kBorderIdle;
    ui.Panel(x, y, w, h, fill, border);
    ui.TextCentered(x + w * 0.5f, y + (h - 7.0f * scale) * 0.5f, scale,
                    label, enabled ? kText : kMuted);
}

} // namespace

void MainMenu::Draw(UiDrawList& ui, float width, float height, bool hasSave) const {
    ui.Rect(0.0f, 0.0f, width, height, kBackdrop);
    ui.Rect(0.0f, height * 0.30f, width, height * 0.52f, kBand);

    const float titleY = height * 0.10f;
    ui.TextCentered(width * 0.5f, titleY, 6.0f, "BLOCKFORGE", kTitle);
    ui.TextCentered(width * 0.5f, titleY + 7.0f * 6.0f + 14.0f, 1.8f,
                    "TWO REALMS  -  ONE RIFT", kMuted);

    if (screen_ == Screen::Root) {
        const float rowWidth = 420.0f;
        const float rowHeight = 54.0f;
        const float gap = 16.0f;
        const float x = (width - rowWidth) * 0.5f;
        float y = height * 0.44f;

        DrawButton(ui, x, y, rowWidth, rowHeight, "CONTINUE WORLD", selection_ == 0, hasSave);
        y += rowHeight + gap;
        DrawButton(ui, x, y, rowWidth, rowHeight, "NEW WORLD", selection_ == 1, true);
        y += rowHeight + gap;
        DrawButton(ui, x, y, rowWidth, rowHeight, "QUIT", selection_ == 2, true);

        ui.TextCentered(width * 0.5f, height * 0.83f, 1.6f,
                        "UP/DOWN  SELECT     ENTER  CONFIRM", kMuted);
    } else {
        const float rowWidth = 560.0f;
        const float rowHeight = 48.0f;
        const float gap = 12.0f;
        const float x = (width - rowWidth) * 0.5f;
        float y = height * 0.30f;

        ui.TextCentered(width * 0.5f, height * 0.17f, 3.0f, "CREATE WORLD", kTitle);

        DrawRow(ui, x, y, rowWidth, rowHeight, "SEED", seedText_, selection_ == 0);
        y += rowHeight + gap;
        DrawRow(ui, x, y, rowWidth, rowHeight, "RENDER DISTANCE",
                std::to_string(options_.renderDistance) + " CHUNKS", selection_ == 1);
        y += rowHeight + gap;
        DrawRow(ui, x, y, rowWidth, rowHeight, "STARTING REALM",
                RealmLabel(options_.startingRealm), selection_ == 2);
        y += rowHeight + gap + 8.0f;

        const float buttonWidth = (rowWidth - gap) * 0.5f;
        DrawButton(ui, x, y, buttonWidth, rowHeight, "CREATE WORLD", selection_ == 3, true);
        DrawButton(ui, x + buttonWidth + gap, y, buttonWidth, rowHeight, "BACK", selection_ == 4, true);

        ui.TextCentered(width * 0.5f, height * 0.84f, 1.6f,
                        "UP/DOWN  SELECT     LEFT/RIGHT  CHANGE     ENTER  CONFIRM", kMuted);
        ui.TextCentered(width * 0.5f, height * 0.89f, 1.4f,
                        "TYPE DIGITS FOR THE SEED   BACKSPACE ERASES   R RANDOMISES", kMuted);
    }
}

void PauseMenu::Reset() {
    selection_ = 0;
    keys_.Reset();
    // The Escape that opened the pause menu is still held on the next frame;
    // treat it as already down so it does not instantly resume the game.
    keys_.Latch(GLFW_KEY_ESCAPE);
}

PauseAction PauseMenu::Update(const InputState& input) {
    PauseAction action = PauseAction::None;

    if (keys_.Pressed(input, GLFW_KEY_ESCAPE)) {
        action = PauseAction::Resume;
    }
    if (keys_.Pressed(input, GLFW_KEY_UP)) {
        selection_ = (selection_ + 2) % 3;
    }
    if (keys_.Pressed(input, GLFW_KEY_DOWN)) {
        selection_ = (selection_ + 1) % 3;
    }
    if (keys_.Pressed(input, GLFW_KEY_ENTER) || keys_.Pressed(input, GLFW_KEY_KP_ENTER)) {
        if (selection_ == 0) {
            action = PauseAction::Resume;
        } else if (selection_ == 1) {
            action = PauseAction::SaveAndTitle;
        } else {
            action = PauseAction::QuitGame;
        }
    }

    keys_.Sync(input);
    return action;
}

void PauseMenu::Draw(UiDrawList& ui, float width, float height) const {
    // Dim the frozen world behind the menu.
    ui.Rect(0.0f, 0.0f, width, height, { 0.02f, 0.03f, 0.05f, 0.58f });

    const float panelWidth = 400.0f;
    const float panelHeight = 300.0f;
    const float panelX = (width - panelWidth) * 0.5f;
    const float panelY = (height - panelHeight) * 0.5f;
    ui.Panel(panelX, panelY, panelWidth, panelHeight,
             { 0.08f, 0.10f, 0.14f, 0.98f }, kBorderActive);

    ui.TextCentered(width * 0.5f, panelY + 24.0f, 3.0f, "PAUSED", kTitle);

    const float buttonWidth = panelWidth - 56.0f;
    const float buttonHeight = 48.0f;
    const float buttonX = panelX + 28.0f;
    float y = panelY + 92.0f;

    DrawButton(ui, buttonX, y, buttonWidth, buttonHeight, "RESUME", selection_ == 0, true);
    y += buttonHeight + 12.0f;
    DrawButton(ui, buttonX, y, buttonWidth, buttonHeight, "SAVE & TITLE", selection_ == 1, true);
    y += buttonHeight + 12.0f;
    DrawButton(ui, buttonX, y, buttonWidth, buttonHeight, "QUIT GAME", selection_ == 2, true);

    ui.TextCentered(width * 0.5f, panelY + panelHeight - 30.0f, 1.5f,
                    "UP/DOWN  SELECT     ENTER  CONFIRM     ESC  RESUME", kMuted);
}
