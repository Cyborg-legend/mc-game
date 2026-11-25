#pragma once

#include <string>
#include <vector>

struct UiColor {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

// Immediate-mode 2D drawing list. Coordinates are pixels with the origin in the
// top-left corner of the window, matching the renderer's UI shader.
//
// Text uses a 5x7 bitmap font baked into ui.cpp, so the game needs no font
// files and no third-party typefaces.
class UiDrawList {
public:
    void Clear();

    void Rect(float x, float y, float w, float h, const UiColor& color);
    void Panel(float x, float y, float w, float h, const UiColor& fill, const UiColor& border);

    void Text(float x, float y, float scale, const std::string& text, const UiColor& color);
    void TextCentered(float centerX, float y, float scale, const std::string& text, const UiColor& color);

    float TextWidth(const std::string& text, float scale) const;

    const std::vector<float>& Vertices() const { return vertices_; }

private:
    // Six floats per vertex: x, y, r, g, b, a.
    std::vector<float> vertices_;
};
