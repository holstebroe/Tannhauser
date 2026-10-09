#ifndef TANNHAUSER_PANEL_LAYOUT_HPP
#define TANNHAUSER_PANEL_LAYOUT_HPP

// The CS-80-style panel as one table of controls and framed groups
// (spec 06 §2). Coordinates are logical window pixels.

#include <cstdint>
#include <vector>

namespace tannhauser {

enum class CtlType {
    Slider,       // programming slider (up = more)
    Paddle,       // performance paddle (pulled down = more, as on the hardware)
    Rocker,       // on/off rocker (down = on)
    Lever,        // stepped selector, `steps` positions listed top to bottom
    Knob,
    ToneButton,   // aux = row * 16 + button (0..13)
    PresetLcd,
    PresetPrev,
    PresetNext,
    Ribbon,
    Keyboard,
};

struct Ctl {
    CtlType type;
    int param;            // parameter id, or -1
    int x, y, w, h;       // hit rectangle
    const char* label;
    uint32_t cap;         // cap colour (ARGB)
    int aux;              // type-specific (lever steps, tone button id)
    const char* const* stepLabels;   // Lever: one label per step
};

struct Frame {
    int x, y, w, h;
    const char* title;
};

struct PanelLayout {
    static constexpr int kWidth = 1360;
    static constexpr int kHeight = 620;
    static constexpr int kCheek = 14;          // walnut side cheeks
    static constexpr int kHeaderH = 36;
    static constexpr int kKeyboardFirst = 36;  // C2 (MIDI 36) .. C7 (96): 61 keys
    static constexpr int kKeyboardLast = 96;
    std::vector<Ctl> controls;
    std::vector<Frame> frames;
    int rowTop[2]{};               // programming rows
    int middleTop = 0, middleH = 0;
    int bottomTop = 0;
};

const PanelLayout& panelLayout();

// Cap colours (spec 06 §1).
constexpr uint32_t kCapGreen = 0xFF3C9A4E;
constexpr uint32_t kCapRed = 0xFFC8322D;
constexpr uint32_t kCapWhite = 0xFFE8E6DE;
constexpr uint32_t kCapGrey = 0xFF9A9C9E;
constexpr uint32_t kCapYellow = 0xFFE7C531;
constexpr uint32_t kCapBlack = 0xFF2A2B2D;
constexpr uint32_t kCapBlue = 0xFF3D6FB0;

} // namespace tannhauser

#endif
