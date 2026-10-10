#include "PanelRenderer.hpp"
#include "core/Params.hpp"
#include "modern/ModernDraw.hpp"
#include "modern/LabelFont.hpp"
#include "modern/DisplayFont.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace tannhauser {
namespace panel {

using namespace modern;

namespace {

constexpr uint32_t kSilk = 0xF2E9E8E2;       // white silk-screen ink
constexpr uint32_t kSilkDim = 0xC0A8AAA6;
constexpr uint32_t kFrameInk = 0xB08E908C;
constexpr float kLabelCap = 6.0f;

const Color kPanelBase{ 0.105f, 0.108f, 0.115f };
const Color kStripBase{ 0.150f, 0.152f, 0.158f };

void label(Graphics& g, const char* text, float cx, float capTop, float cap = kLabelCap, uint32_t ink = kSilk) {
    drawTextCentered(g, kLabelFont, text, cx, capTop, cap, ink, 0.08f);
}

// Waveform symbols printed on the CS-80 (pulse, saw, sine), cap-height tall.
void waveSymbol(Graphics& g, const char* name, float cx, float capTop, float cap, uint32_t ink) {
    const float y0 = capTop, y1 = capTop + cap, w = cap * 1.6f, lw = 0.9f;
    if (std::strcmp(name, "SQ") == 0) {
        const float xs[6] = { cx - w * 0.5f, cx - w * 0.2f, cx - w * 0.2f, cx + w * 0.2f, cx + w * 0.2f, cx + w * 0.5f };
        const float ys[6] = { y1, y1, y0, y0, y1, y1 };
        for (int i = 0; i < 5; ++i) drawLineAA(g, xs[i], ys[i], xs[i + 1], ys[i + 1], lw, ink);
    } else if (std::strcmp(name, "SAW") == 0) {
        drawLineAA(g, cx - w * 0.4f, y1, cx + w * 0.3f, y0, lw, ink);
        drawLineAA(g, cx + w * 0.3f, y0, cx + w * 0.3f, y1, lw, ink);
    } else {   // SINE
        const int n = 16;
        for (int i = 0; i < n; ++i) {
            const float a = static_cast<float>(i) / n, b = static_cast<float>(i + 1) / n;
            drawLineAA(g, cx - w * 0.5f + a * w, 0.5f * (y0 + y1) - 0.5f * cap * std::sin(a * 6.2831853f),
                       cx - w * 0.5f + b * w, 0.5f * (y0 + y1) - 0.5f * cap * std::sin(b * 6.2831853f), lw, ink);
        }
    }
}

// A control name at the one label size: "~SQ"-style waveform symbols, "RES_H" with a
// subscript. Names are kept short enough for their column (nameWidth, GUI test).
void nameLabel(Graphics& g, const char* text, float cx, float capTop, float cap = kLabelCap, uint32_t ink = kSilk) {
    if (!text || !*text) return;
    if (text[0] == '~') { waveSymbol(g, text + 1, cx, capTop, cap, ink); return; }
    char main[32];
    const char* sub = std::strchr(text, '_');
    const size_t n = sub ? static_cast<size_t>(sub - text) : std::strlen(text);
    std::snprintf(main, sizeof main, "%.*s", static_cast<int>(n), text);
    const float w = nameWidth(text);
    drawText(g, kLabelFont, main, cx - w * 0.5f, capTop, cap, ink, 0.08f);
    if (sub) drawText(g, kLabelFont, sub + 1, cx - w * 0.5f + textWidth(kLabelFont, main, cap) + 1.f, capTop + cap * 0.55f,
                      cap * 0.7f, ink, 0.08f);
}

void fillRect(Graphics& g, float x0, float y0, float x1, float y1, uint32_t argb) {
    shadeBox(g, x0, y0, x1, y1, [argb](float, float) { return argb; });
}

// Rounded rectangle with a per-pixel colour function f(u, v, d): u,v in -1..1
// across the box, d = signed distance (logical px, negative inside).
template <class F>
void roundRect(Graphics& g, float x0, float y0, float x1, float y1, float r, F&& f) {
    const float s = static_cast<float>(g.getScale());
    const float cx = 0.5f * (x0 + x1), cy = 0.5f * (y0 + y1);
    const float hx = 0.5f * (x1 - x0), hy = 0.5f * (y1 - y0);
    shadeBox(g, x0 - 1, y0 - 1, x1 + 1, y1 + 1, [&](float x, float y) {
        const float d = sdRoundRect(x - cx, y - cy, hx, hy, r);
        const float cov = clamp01(0.5f - d * s);
        if (cov <= 0.f) return 0u;
        uint32_t c = f((x - cx) / hx, (y - cy) / hy, d);
        const float a = alphaOf(c) * cov;
        return toArgb(fromArgb(c), a);
    });
}

void softShadow(Graphics& g, float x0, float y0, float x1, float y1, float r, float spread, float alpha, float dx, float dy) {
    const float cx = 0.5f * (x0 + x1) + dx, cy = 0.5f * (y0 + y1) + dy;
    const float hx = 0.5f * (x1 - x0), hy = 0.5f * (y1 - y0);
    shadeBox(g, x0 + dx - spread, y0 + dy - spread, x1 + dx + spread, y1 + dy + spread, [&](float x, float y) {
        const float d = sdRoundRect(x - cx, y - cy, hx, hy, r);
        const float a = alpha * (1.f - smoothstep(-spread * 0.3f, spread, d));
        return a > 0.002f ? toArgb({ 0.f, 0.f, 0.f }, a) : 0u;
    });
}

// Slider/paddle slot: a dark recessed groove.
void drawSlot(Graphics& g, float cx, float y0, float y1, float w) {
    roundRect(g, cx - w / 2, y0, cx + w / 2, y1, w / 2, [&](float, float v, float d) {
        const float rim = smoothstep(-1.2f, 0.f, d);
        const Color c = mix({ 0.015f, 0.015f, 0.018f }, { 0.30f, 0.31f, 0.32f }, rim * (v > 0 ? 0.9f : 0.3f));
        return toArgb(c, 1.f);
    });
}

void drawTicks(Graphics& g, float x, float y0, float y1, bool center) {
    for (int i = 0; i <= 10; ++i) {
        const float y = y0 + (y1 - y0) * i / 10.f;
        const float len = (i % 5 == 0) ? 4.f : 2.5f;
        const uint32_t ink = (center && i == 5) ? kSilk : kSilkDim;
        drawLineAA(g, x - len, y, x, y, 0.8f, ink);
    }
}

void sliderGeometry(const Ctl& c, float& cx, float& top, float& bottom) {
    cx = c.x + c.w * 0.5f;
    top = c.y + 7.f;
    bottom = static_cast<float>(c.y + c.h) - 7.f;
}

bool isBipolar(int param) {
    return param >= 0 && (paramInfo(static_cast<uint32_t>(param)).flags & PF_BIPOLAR) != 0;
}

// Tone-button colours by family (photo docs/reference/cs80.jpg).
uint32_t toneColour(int button) {
    static const uint32_t k[14] = { 0xFFE9CF3A, 0xFFE9CF3A, 0xFFD23A2E, 0xFFE9E3C9, 0xFFE9E3C9, 0xFF4E8DCF,
                                    0xFF4E8DCF, 0xFF4BA35A, 0xFFE58B2E, 0xFF3FB8B0, 0xFF3FB8B0, 0xFF9DA0A3,
                                    0xFF9DA0A3, 0xFFF2F0EA };
    return k[button < 0 ? 0 : (button > 13 ? 13 : button)];
}

const char* const kToneTop[2][14] = {
    { "STR", "STR", "BRASS", "FLUTE", "E.PNO", "CLAV", "HARPS", "ORGAN", "GUIT.", "FUNKY", "FUNKY", "MEM", "MEM", "PANEL" },
    { "STR", "STR", "BRASS", "BRASS", "BASS", "CLAV", "HARPS", "ORGAN", "GUIT.", "FUNKY", "FUNKY", "MEM", "MEM", "PANEL" },
};
const char* const kToneBottom[2][14] = {
    { "1", "3", "1", "", "", "1", "1", "1", "1", "1", "3", "1", "3", "" },
    { "2", "4", "2", "3", "", "2", "2", "2", "2", "2", "4", "2", "4", "" },
};

void drawKnobScale(Graphics& g, float cx, float cy, float r, bool bipolar) {
    const float start = 120.f * kPi / 180.f, total = 300.f * kPi / 180.f;
    for (int i = 0; i <= 10; ++i) {
        const float a = start + total * i / 10.f;
        const float len = (i % 5 == 0) ? 5.f : 3.f;
        drawLineAA(g, cx + std::cos(a) * (r + 3.f), cy + std::sin(a) * (r + 3.f),
                   cx + std::cos(a) * (r + 3.f + len), cy + std::sin(a) * (r + 3.f + len), 1.0f,
                   (bipolar && i == 5) ? kSilk : kSilkDim);
    }
}

} // namespace

const char* toneButtonTop(int row, int button) { return kToneTop[row & 1][button % 14]; }
const char* toneButtonBottom(int row, int button) { return kToneBottom[row & 1][button % 14]; }

// --- Static layer -----------------------------------------------------------------------------

void drawStatic(Graphics& g, const PanelLayout& L) {
    const int W = PanelLayout::kWidth, H = PanelLayout::kHeight;
    const int s = g.getScale();
    // Charcoal panel with a faint grain; the middle strip a shade lighter.
    uint32_t* buf = g.getBuffer();
    const int bw = W * s, bh = H * s;
    for (int by = 0; by < bh; ++by) {
        const float ly = (by + 0.5f) / s;
        const bool strip = ly >= L.middleTop && ly < L.middleTop + L.middleH;
        const Color base = strip ? kStripBase : kPanelBase;
        for (int bx = 0; bx < bw; ++bx) {
            const float n = hash2(bx, by, 11) * 0.025f - 0.0125f;
            const Color c{ base.r + n, base.g + n, base.b + n * 1.1f };
            buf[static_cast<size_t>(by) * bw + bx] = toArgb(c, 1.f);
        }
    }
    // Chrome lines around the middle strip.
    for (float y : { static_cast<float>(L.middleTop), static_cast<float>(L.middleTop + L.middleH) }) {
        fillRect(g, PanelLayout::kCheek, y - 1.f, W - PanelLayout::kCheek, y + 1.f, 0xFF6E7072);
        fillRect(g, PanelLayout::kCheek, y - 1.f, W - PanelLayout::kCheek, y - 0.5f, 0xFFB4B6B8);
    }
    // Walnut cheeks.
    for (int side = 0; side < 2; ++side) {
        const float x0 = side == 0 ? 0.f : static_cast<float>(W - PanelLayout::kCheek);
        shadeBox(g, x0, 0, x0 + PanelLayout::kCheek, static_cast<float>(H), [&](float x, float y) {
            const float grain = std::sin((x * 0.9f + fbm(x * 0.05f, y * 0.012f, 5) * 9.f) * 2.2f);
            const float t = 0.5f + 0.5f * grain;
            Color c = mix({ 0.36f, 0.19f, 0.09f }, { 0.52f, 0.29f, 0.14f }, t * 0.8f);
            const float u = (x - x0) / PanelLayout::kCheek;
            const float edge = side == 0 ? u : 1.f - u;
            c = scale(c, 0.75f + 0.35f * std::sin(edge * kPi));
            return toArgb(c, 1.f);
        });
    }
    // Header: logo and tagline.
    drawText(g, kDisplayFont, "TANNHAUSER", 26.f, 10.f, 17.f, 0xFFF0EEE6);
    // The umlaut over the first A.
    {
        const float ax = 26.f + textWidth(kDisplayFont, "TANNH", 17.f);
        const float aw = textWidth(kDisplayFont, "A", 17.f);
        for (float dx : { 0.33f, 0.67f }) {
            const float cx = ax + aw * dx, cy = 6.5f;
            shadeBox(g, cx - 2, cy - 2, cx + 2, cy + 2, [&](float x, float y) {
                const float d = std::hypot(x - cx, y - cy);
                return toArgb({ 0.94f, 0.93f, 0.90f }, clamp01((1.5f - d) * s + 0.5f));
            });
        }
    }
    drawText(g, kLabelFont, "POLYPHONIC SYNTHESIZER", 214.f, 15.f, 6.5f, kSilkDim);

    // Row markers.
    for (int row = 0; row < 2; ++row) {
        drawText(g, kDisplayFont, row == 0 ? "I" : "II", row == 0 ? 25.f : 21.f, L.rowTop[row] + 56.f, 16.f, kSilk);
    }

    // Frames with titles cut into the top line.
    for (const Frame& f : L.frames) {
        const float x0 = f.x + 0.5f, y0 = f.y + 6.5f, x1 = f.x + f.w - 0.5f, y1 = f.y + f.h - 0.5f;
        const float tw = (f.title && *f.title) ? textWidth(kLabelFont, f.title, 6.5f) + 8.f : 0.f;
        const float cx = 0.5f * (x0 + x1);
        if (tw > 0.f) {
            drawLineAA(g, x0, y0, cx - tw / 2, y0, 1.f, kFrameInk);
            drawLineAA(g, cx + tw / 2, y0, x1, y0, 1.f, kFrameInk);
            label(g, f.title, cx, f.y + 3.f, 6.5f);
        } else {
            drawLineAA(g, x0, y0, x1, y0, 1.f, kFrameInk);
        }
        drawLineAA(g, x0, y0, x0, y1, 1.f, kFrameInk);
        drawLineAA(g, x1, y0, x1, y1, 1.f, kFrameInk);
        drawLineAA(g, x0, y1, x1, y1, 1.f, kFrameInk);
    }

    // Per-control printing: labels, slots, scales.
    for (const Ctl& c : L.controls) {
        const float cx = c.x + c.w * 0.5f;
        switch (c.type) {
            case CtlType::Slider:
            case CtlType::Paddle: {
                float sx, top, bottom;
                sliderGeometry(c, sx, top, bottom);
                nameLabel(g, c.label, cx, c.nameY ? static_cast<float>(c.nameY) : c.y - 13.f);
                drawTicks(g, c.x + 3.f, top, bottom, isBipolar(c.param));
                drawSlot(g, sx, top - 3.f, bottom + 3.f, c.type == CtlType::Slider ? 5.f : 7.f);
                if (c.type == CtlType::Paddle) {
                    // Reversed scale: the hardware's paddles grow toward the player. The top
                    // legend replaces the "0" mark; the bottom one is printed under the slot.
                    drawText(g, kLabelFont, c.top ? c.top : "0", c.x + c.w - 4.f, top - 2.f, 4.5f, kSilkDim);
                    if (c.bottom) label(g, c.bottom, cx, bottom + 7.f, 4.5f, kSilkDim);
                } else {
                    if (c.top) label(g, c.top, cx, c.y - 6.f, 4.5f, kSilkDim);
                    if (c.bottom) label(g, c.bottom, cx, c.y + c.h + 1.f, 4.5f, kSilkDim);
                }
                break;
            }
            case CtlType::Rocker:
                if (c.nameY) nameLabel(g, c.label, cx, static_cast<float>(c.nameY));
                else nameLabel(g, c.label, cx, c.y - 13.f - (c.top ? 6.f : 0.f));
                roundRect(g, c.x - 2.f, c.y - 2.f, c.x + c.w + 2.f, c.y + c.h + 2.f, 3.f,
                          [](float, float, float) { return 0xFF050506u; });
                if (c.top) label(g, c.top, cx, c.y - 10.f, 4.5f, kSilkDim);
                if (c.bottom) label(g, c.bottom, cx, c.y + c.h + 5.f, 4.5f, kSilkDim);
                else if (!c.top) label(g, "ON", cx, c.y + c.h + 5.f, 4.5f, kSilkDim);
                break;
            case CtlType::Toggle:   // the letter is part of the key (dynamic layer)
                roundRect(g, c.x - 2.f, c.y - 2.f, c.x + c.w + 2.f, c.y + c.h + 2.f, 3.f,
                          [](float, float, float) { return 0xFF050506u; });
                break;
            case CtlType::Lever: {
                label(g, c.label, cx, c.y - 13.f);
                const float slotX = c.x + 7.f;
                drawSlot(g, slotX, c.y + 2.f, c.y + c.h - 2.f, 5.f);
                break;
            }
            case CtlType::Knob: {
                const float r = c.w * 0.5f - 2.f;
                drawKnobScale(g, cx, c.y + c.h * 0.5f, r, isBipolar(c.param));
                break;
            }
            default: break;
        }
    }
    for (const Caption& k : L.captions) label(g, k.text, k.x, k.y, k.cap, k.cap < 5.f ? kSilkDim : kSilk);
    // The YAMAHA-style name board above the keyboard is the ribbon; print a
    // small credit under the tone selector.
    label(g, "SOFTWARE TONE SELECTOR - CLICK THE DISPLAY FOR PRESETS", 618.f, L.middleTop + 143.f, 4.6f, kSilkDim);
}

// --- Dynamic controls -----------------------------------------------------------------------

void controlBounds(const Ctl& c, int& x, int& y, int& w, int& h) {
    int m = 4;
    if (c.type == CtlType::Lever) m = 2;
    x = c.x - m; y = c.y - m; w = c.w + 2 * m; h = c.h + 2 * m;
    if (c.type == CtlType::Slider || c.type == CtlType::Paddle) { x = c.x + 4; w = c.w - 4 + m; }
    if (c.type == CtlType::Lever) { x = c.x + 1; w = c.w + 1; }
}

void readoutBounds(int& x, int& y, int& w, int& h) { x = 350; y = 4; w = 980; h = 28; }

float nameWidth(const char* text) {
    if (!text || !*text) return 0.f;
    if (text[0] == '~') return kLabelCap * 1.6f;
    char main[32];
    const char* sub = std::strchr(text, '_');
    const size_t n = sub ? static_cast<size_t>(sub - text) : std::strlen(text);
    std::snprintf(main, sizeof main, "%.*s", static_cast<int>(n), text);
    return textWidth(kLabelFont, main, kLabelCap) + (sub ? 1.f + textWidth(kLabelFont, sub + 1, kLabelCap * 0.7f) : 0.f);
}

float nameRoom(const Ctl& c) {
    // Column pitch minus a small gap: programming rows 44, paddle groups 34, rockers 36;
    // a lever has its own width.
    if (c.type == CtlType::Lever || c.type == CtlType::Toggle) return static_cast<float>(c.w);
    if (c.nameY) return 42.f;
    return c.type == CtlType::Rocker ? 33.f : 31.f;
}

float readoutWidth(const std::string& line, bool first) { return textWidth(kLabelFont, line.c_str(), first ? 7.f : 5.5f); }

static void drawSliderCap(Graphics& g, const Ctl& c, const CtlState& st) {
    float cx, top, bottom;
    sliderGeometry(c, cx, top, bottom);
    const bool paddle = c.type == CtlType::Paddle;
    const double t = paddle ? st.norm : 1.0 - st.norm;   // paddles: down = more
    const float y = top + static_cast<float>(t) * (bottom - top);
    const Color capC = fromArgb(c.cap);
    if (!paddle) {
        const float hw = 10.5f, hh = 5.5f;
        softShadow(g, cx - hw, y - hh, cx + hw, y + hh, 2.f, 3.f, 0.55f, 1.f, 2.f);
        roundRect(g, cx - hw, y - hh, cx + hw, y + hh, 2.0f, [&](float, float v, float d) {
            const float rim = smoothstep(-1.4f, 0.f, d);
            Color col = scale(capC, 1.08f - 0.25f * v);
            col = mix(col, scale(capC, v < 0 ? 1.35f : 0.6f), rim * 0.6f);
            if (st.active || st.hover) col = scale(col, 1.08f);
            return toArgb(col, 1.f);
        });
        // Index line.
        drawLineAA(g, cx - hw + 2.f, y, cx + hw - 2.f, y, 1.1f, c.cap == kCapBlack ? 0xFFE0E0DA : 0xFF151515);
    } else {
        const float hw = 9.5f, hh = 8.f;
        softShadow(g, cx - hw, y - hh, cx + hw, y + hh, 2.f, 3.5f, 0.6f, 1.f, 2.5f);
        roundRect(g, cx - hw, y - hh, cx + hw, y + hh, 2.5f, [&](float, float v, float d) {
            const float rim = smoothstep(-1.5f, 0.f, d);
            // Grey paddle with a coloured lip at the bottom (the end you pull).
            Color col = v > 0.45f ? capC : Color{ 0.78f, 0.79f, 0.80f };
            col = scale(col, 1.05f - 0.25f * v);
            const float ridge = 0.5f + 0.5f * std::cos((v + 1.f) * 5.f * kPi);
            if (v < 0.4f) col = scale(col, 0.92f + 0.08f * ridge);
            col = mix(col, scale(col, v < 0 ? 1.3f : 0.55f), rim * 0.6f);
            if (st.active || st.hover) col = scale(col, 1.08f);
            return toArgb(col, 1.f);
        });
    }
}

static void drawRockerKey(Graphics& g, const Ctl& c, const CtlState& st) {
    const float x0 = c.x + 1.f, y0 = c.y + 1.f, x1 = c.x + c.w - 1.f, y1 = c.y + c.h - 1.f;
    const Color capC = fromArgb(c.cap);
    // Down = on: the pressed half is in shadow, the other half catches the light.
    roundRect(g, x0, y0, x1, y1, 2.f, [&](float u, float v, float d) {
        const bool upper = v < 0.f;
        const bool raised = st.on ? upper : !upper;
        float k = raised ? 1.08f - 0.15f * std::fabs(v) : 0.62f + 0.1f * std::fabs(v);
        if (raised && (upper ? v < -0.85f : v > 0.85f)) k = 1.25f;
        Color col = scale(capC, k);
        const float rim = smoothstep(-1.2f, 0.f, d);
        col = mix(col, scale(col, u < 0 ? 1.15f : 0.75f), rim * 0.5f);
        if (st.hover) col = scale(col, 1.06f);
        return toArgb(col, 1.f);
    });
    drawLineAA(g, x0 + 1.f, c.y + c.h * 0.5f, x1 - 1.f, c.y + c.h * 0.5f, 0.8f, 0x60000000);
    // A small lamp under lit switches for readability.
    const float lx = c.x + c.w * 0.5f, ly = y1 - 5.f;
    if (st.on) {
        shadeBox(g, lx - 5, ly - 5, lx + 5, ly + 5, [&](float x, float y) {
            const float dd = std::hypot(x - lx, y - ly);
            return toArgb({ 1.0f, 0.22f, 0.12f }, clamp01(1.25f - dd / 3.f));
        });
    }
}

// Small square key with its letter lit green when on (Ribbon Hold "H").
static void drawToggle(Graphics& g, const Ctl& c, const CtlState& st) {
    const float x0 = c.x + 1.f, y0 = c.y + 1.f, x1 = c.x + c.w - 1.f, y1 = c.y + c.h - 1.f;
    const Color capC = fromArgb(c.cap);
    roundRect(g, x0, y0, x1, y1, 2.5f, [&](float u, float v, float d) {
        float k = st.on ? 0.75f - 0.1f * v : 1.15f - 0.2f * v;   // pressed in when on
        const float rim = smoothstep(-1.2f, 0.f, d);
        Color col = scale(capC, k);
        col = mix(col, scale(col, (u < 0) == !st.on ? 1.4f : 0.6f), rim * 0.6f);
        if (st.hover) col = scale(col, 1.1f);
        return toArgb(col, 1.f);
    });
    drawTextCentered(g, kLabelFont, c.label, c.x + c.w * 0.5f, c.y + c.h * 0.5f - 3.5f, 7.f,
                     st.on ? 0xFF7DF29A : 0xC0A8AAA6, 0.08f);
}

static void drawLever(Graphics& g, const Ctl& c, const CtlState& st) {
    const int n = std::max(1, c.aux);
    const float slotX = c.x + 7.f;
    const float step = (c.h - 8.f) / n;
    for (int i = 0; i < n; ++i) {
        const float y = c.y + 4.f + step * (i + 0.5f);
        const bool sel = i == st.step;
        drawText(g, kLabelFont, c.stepLabels ? c.stepLabels[i] : "", slotX + 9.f, y - 2.6f, 5.2f,
                 sel ? 0xFFFFFFFF : kSilkDim);
        drawLineAA(g, slotX + 3.5f, y, slotX + 6.5f, y, 0.8f, kSilkDim);
        if (sel) {
            const float hw = 5.f, hh = 4.f;
            softShadow(g, slotX - hw, y - hh, slotX + hw, y + hh, 1.5f, 2.5f, 0.5f, 1.f, 1.5f);
            const Color capC = fromArgb(c.cap);
            roundRect(g, slotX - hw, y - hh, slotX + hw, y + hh, 1.5f, [&](float, float v, float) {
                return toArgb(scale(capC, 1.05f - 0.3f * v), 1.f);
            });
        }
    }
}

static void drawKnob(Graphics& g, const Ctl& c, const CtlState& st) {
    const float s = static_cast<float>(g.getScale());
    const float cx = c.x + c.w * 0.5f, cy = c.y + c.h * 0.5f, R = c.w * 0.5f - 3.f;
    const float a = (120.f + 300.f * static_cast<float>(st.norm)) * kPi / 180.f;
    const float px = std::cos(a), py = std::sin(a);
    softShadow(g, cx - R, cy - R, cx + R, cy + R, R, 4.f, 0.6f, 1.5f, 2.5f);
    const Material skirt{ { 0.10f, 0.10f, 0.11f }, 0.5f, 0.6f, 0.4f, 18.f };
    const Material cap{ { 0.72f, 0.73f, 0.75f }, 0.35f, 0.7f, 0.6f, 30.f };
    shadeBox(g, cx - R - 1, cy - R - 1, cx + R + 1, cy + R + 1, [&](float x, float y) {
        const float dx = x - cx, dy = y - cy, d = std::sqrt(dx * dx + dy * dy);
        const float cov = clamp01((R - d) * s + 0.5f);
        if (cov <= 0.f) return 0u;
        const float ux = d > 1e-4f ? dx / d : 0.f, uy = d > 1e-4f ? dy / d : 0.f;
        Color col;
        if (d > 0.62f * R) {
            const float th = std::atan2(dy, dx);
            const float ridge = std::sin(24.f * th);
            col = shadeLit(skirt, normalize({ ux * 0.8f - uy * 0.3f * ridge, uy * 0.8f + ux * 0.3f * ridge, 0.6f }));
        } else {
            const float r2 = d / (0.62f * R);
            col = shadeLit(cap, tiltedNormal(ux, uy, 0.25f * r2));
            // Spun-metal rings.
            col = scale(col, 0.95f + 0.05f * std::sin(d * 9.f));
        }
        // Pointer line.
        const float along = dx * px + dy * py, perp = std::fabs(-dx * py + dy * px);
        if (along > 0.15f * R && along < R - 1.f && perp < 1.1f) col = { 0.95f, 0.95f, 0.93f };
        return toArgb(col, cov);
    });
}

static void drawToneButton(Graphics& g, const Ctl& c, const CtlState& st) {
    const int row = c.aux / 16, b = c.aux % 16;
    const Color base = fromArgb(toneColour(b));
    const bool lit = st.on;
    softShadow(g, c.x, c.y, c.x + c.w, c.y + c.h, 2.f, 3.f, 0.5f, 1.f, 2.f);
    roundRect(g, c.x, c.y, c.x + c.w, c.y + c.h, 2.f, [&](float u, float v, float d) {
        Color col = scale(base, lit ? 1.05f : 0.52f);
        col = scale(col, 1.02f - 0.12f * v);
        if (lit) col = mix(col, { 1.f, 1.f, 0.95f }, 0.25f * (1.f - std::fabs(u)) * (1.f - std::fabs(v)));
        const float rim = smoothstep(-1.3f, 0.f, d);
        col = mix(col, scale(col, v < 0 ? 1.25f : 0.6f), rim * 0.5f);
        if (st.hover) col = scale(col, 1.08f);
        return toArgb(col, 1.f);
    });
    const uint32_t ink = lit ? 0xF0181818 : 0xC0101010;
    drawTextCentered(g, kLabelFont, toneButtonTop(row, b), c.x + c.w * 0.5f, c.y + 10.f, 4.4f, ink);
    drawTextCentered(g, kLabelFont, toneButtonBottom(row, b), c.x + c.w * 0.5f, c.y + 20.f, 5.5f, ink);
    if ((b == 11 || b == 12) && st.hasMemory) {
        const float lx = c.x + c.w * 0.5f, ly = c.y + c.h - 6.f;
        shadeBox(g, lx - 3, ly - 3, lx + 3, ly + 3, [&](float x, float y) {
            return toArgb({ 0.15f, 0.85f, 0.3f }, clamp01(1.f - std::hypot(x - lx, y - ly) / 2.f));
        });
    }
}

static void drawLcd(Graphics& g, const Ctl& c, const CtlState& st) {
    roundRect(g, c.x, c.y, c.x + c.w, c.y + c.h, 3.f, [&](float, float v, float d) {
        Color col{ 0.02f, 0.055f, 0.03f };
        col = scale(col, 1.f + 0.4f * v);
        const float rim = smoothstep(-1.5f, 0.f, d);
        col = mix(col, { 0.25f, 0.26f, 0.27f }, rim);
        return toArgb(col, 1.f);
    });
    std::string t = st.text + (st.modified ? " *" : "");
    float cap = 10.f;
    while (cap > 6.f && textWidth(kDisplayFont, t.c_str(), cap) > c.w - 14.f) cap -= 0.5f;
    const float tw = textWidth(kDisplayFont, t.c_str(), cap);
    // Glow, then the text.
    drawText(g, kDisplayFont, t.c_str(), c.x + (c.w - tw) * 0.5f, c.y + (c.h - cap) * 0.5f, cap,
             st.hover ? 0xFF8CFF9C : 0xFF45E86A);
}

static void drawArrowButton(Graphics& g, const Ctl& c, const CtlState& st, bool left) {
    roundRect(g, c.x, c.y, c.x + c.w, c.y + c.h, 3.f, [&](float, float v, float d) {
        Color col = st.active ? Color{ 0.12f, 0.12f, 0.13f } : Color{ 0.22f, 0.23f, 0.24f };
        col = scale(col, 1.1f - 0.3f * v);
        if (st.hover) col = scale(col, 1.25f);
        const float rim = smoothstep(-1.2f, 0.f, d);
        return toArgb(mix(col, { 0.45f, 0.46f, 0.47f }, rim * 0.6f), 1.f);
    });
    const float cx = c.x + c.w * 0.5f, cy = c.y + c.h * 0.5f, k = left ? -1.f : 1.f;
    drawLineAA(g, cx - 3.f * k, cy - 5.f, cx + 3.f * k, cy, 1.6f, 0xFFE8E8E2);
    drawLineAA(g, cx + 3.f * k, cy, cx - 3.f * k, cy + 5.f, 1.6f, 0xFFE8E8E2);
}

static void drawRibbon(Graphics& g, const Ctl& c, const CtlState& st) {
    roundRect(g, c.x, c.y, c.x + c.w, c.y + c.h, 3.f, [&](float, float v, float d) {
        Color col = mix({ 0.30f, 0.31f, 0.33f }, { 0.10f, 0.10f, 0.11f }, 0.5f + 0.5f * v);
        col = scale(col, 1.f + 0.15f * std::exp(-v * v * 8.f));
        const float rim = smoothstep(-1.2f, 0.f, d);
        return toArgb(mix(col, { 0.55f, 0.56f, 0.57f }, rim * 0.5f), 1.f);
    });
    if (st.ribbonTouch >= 0.0) {
        const float tx = c.x + static_cast<float>(st.ribbonTouch) * c.w;
        const float ox = tx - static_cast<float>(st.ribbonValue) * c.w * 0.5f;   // where the touch started
        drawLineAA(g, ox, c.y + 3.f, ox, c.y + c.h - 3.f, 1.f, 0x80FFFFFF);
        shadeBox(g, tx - 10, c.y, tx + 10, c.y + c.h, [&](float x, float) {
            return toArgb({ 1.f, 0.85f, 0.4f }, 0.6f * std::exp(-(x - tx) * (x - tx) / 18.f));
        });
    }
}

static bool isBlack(int key) {
    const int n = key % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

static void drawKeyboard(Graphics& g, const Ctl& c, const CtlState& st) {
    int whites = 0;
    for (int k = PanelLayout::kKeyboardFirst; k <= PanelLayout::kKeyboardLast; ++k) whites += isBlack(k) ? 0 : 1;
    const float ww = static_cast<float>(c.w) / whites;
    float x = static_cast<float>(c.x);
    // Felt strip behind the keys.
    fillRect(g, c.x, c.y, c.x + c.w, c.y + 3.f, 0xFF5A1E1A);
    for (int k = PanelLayout::kKeyboardFirst; k <= PanelLayout::kKeyboardLast; ++k) {
        if (isBlack(k)) continue;
        const bool down = st.keys && st.keys[k];
        const float x0 = x + 0.5f, x1 = x + ww - 0.5f;
        shadeBox(g, x0, c.y + 3.f, x1, c.y + c.h, [&](float xx, float y) {
            const float v = (y - c.y) / c.h;
            Color col = down ? Color{ 0.80f, 0.76f, 0.62f } : Color{ 0.94f, 0.92f, 0.86f };
            col = scale(col, 0.88f + 0.12f * v);
            if (xx - x0 < 1.f || x1 - xx < 1.f) col = scale(col, 0.7f);
            if (y > c.y + c.h - 4.f) col = scale(col, down ? 0.7f : 0.82f);
            return toArgb(col, 1.f);
        });
        x += ww;
    }
    x = static_cast<float>(c.x);
    for (int k = PanelLayout::kKeyboardFirst; k <= PanelLayout::kKeyboardLast; ++k) {
        if (!isBlack(k)) { x += ww; continue; }
        const bool down = st.keys && st.keys[k];
        const float bw = ww * 0.62f, x0 = x - bw * 0.5f, x1 = x + bw * 0.5f, y1 = c.y + c.h * 0.62f;
        softShadow(g, x0, c.y + 3.f, x1, y1, 1.f, 2.5f, 0.5f, 1.f, 1.5f);
        shadeBox(g, x0, c.y + 3.f, x1, y1, [&](float xx, float y) {
            const float v = (y - c.y) / (y1 - c.y);
            Color col = down ? Color{ 0.28f, 0.22f, 0.12f } : Color{ 0.06f, 0.06f, 0.065f };
            col = scale(col, 1.f + 0.6f * (1.f - std::fabs((xx - x) / (bw * 0.5f))) * (v > 0.85f ? 2.f : 0.4f));
            return toArgb(col, 1.f);
        });
    }
}

void drawControl(Graphics& g, const Ctl& c, const CtlState& st) {
    switch (c.type) {
        case CtlType::Slider:
        case CtlType::Paddle: drawSliderCap(g, c, st); break;
        case CtlType::Rocker: drawRockerKey(g, c, st); break;
        case CtlType::Toggle: drawToggle(g, c, st); break;
        case CtlType::Lever: drawLever(g, c, st); break;
        case CtlType::Knob: drawKnob(g, c, st); break;
        case CtlType::ToneButton: drawToneButton(g, c, st); break;
        case CtlType::PresetLcd: drawLcd(g, c, st); break;
        case CtlType::PresetPrev: drawArrowButton(g, c, st, true); break;
        case CtlType::PresetNext: drawArrowButton(g, c, st, false); break;
        case CtlType::Ribbon: drawRibbon(g, c, st); break;
        case CtlType::Keyboard: drawKeyboard(g, c, st); break;
    }
}

void drawReadout(Graphics& g, const std::string& text) {
    int x, y, w, h;
    readoutBounds(x, y, w, h);
    if (text.empty()) return;
    // "NAME: VALUE" and, after a newline, a one-line description (smaller, dimmer).
    const size_t nl = text.find('\n');
    const std::string first = text.substr(0, nl);
    const float tw = readoutWidth(first, true);
    drawText(g, kLabelFont, first.c_str(), x + w - tw - 4.f, y + 3.f, 7.f, 0xFF7DF29A);
    if (nl != std::string::npos) {
        const std::string second = text.substr(nl + 1);
        const float dw = readoutWidth(second, false);
        drawText(g, kLabelFont, second.c_str(), x + w - dw - 4.f, y + 15.f, 5.5f, 0xC07DF29A);
    }
}

} // namespace panel
} // namespace tannhauser
