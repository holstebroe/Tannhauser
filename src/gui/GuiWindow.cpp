#include "GuiWindow.hpp"
#include "Graphics.hpp"
#include "modern/ModernDraw.hpp"
#include "modern/LabelFont.hpp"
#include "clap/TannhauserClap.hpp"
#include "presets/Presets.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

#if defined(__linux__) && !defined(__APPLE__)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#endif

namespace tannhauser {

namespace {
constexpr int kScale = 2;   // supersampling

bool sameState(const CtlState& a, const CtlState& b) {
    return a.norm == b.norm && a.on == b.on && a.hover == b.hover && a.active == b.active && a.step == b.step
           && a.text == b.text && a.modified == b.modified && a.ribbonTouch == b.ribbonTouch
           && a.ribbonValue == b.ribbonValue && a.hasMemory == b.hasMemory;
}

void margins(const Ctl& c, int& mx, int& my) {
    switch (c.type) {
        case CtlType::Slider: mx = 8; my = 8; break;
        case CtlType::Paddle: mx = 4; my = 8; break;
        case CtlType::Knob: mx = 8; my = 8; break;
        case CtlType::Lever: mx = 3; my = 3; break;
        case CtlType::Rocker: mx = 2; my = 2; break;
        case CtlType::ToneButton: mx = 1; my = 1; break;
        case CtlType::Keyboard: mx = 0; my = 0; break;
        default: mx = 2; my = 2; break;
    }
}
} // namespace

GuiWindow::GuiWindow(TannhauserClap* plugin) : plugin_(plugin) {
    const auto& L = panelLayout();
    drawn_.resize(L.controls.size());
    drawnValid_.assign(L.controls.size(), false);
    pixels_.assign(static_cast<size_t>(width_) * height_, 0xFF1B1C1E);
}

GuiWindow::~GuiWindow() { destroy(); }

// --- State -------------------------------------------------------------------------------------

double GuiWindow::paramNorm(int param) const {
    if (param < 0 || !plugin_) return 0.0;
    const ParamInfo& p = paramInfo(static_cast<uint32_t>(param));
    const double v = plugin_->paramValue(static_cast<clap_id>(param));
    return p.max > p.min ? (v - p.min) / (p.max - p.min) : 0.0;
}

CtlState GuiWindow::stateFor(int i) const {
    const Ctl& c = panelLayout().controls[static_cast<size_t>(i)];
    CtlState s;
    s.hover = (i == hover_);
    s.active = (i == active_);
    const double v = (c.param >= 0 && plugin_) ? plugin_->paramValue(static_cast<clap_id>(c.param)) : 0.0;
    switch (c.type) {
        case CtlType::Slider: case CtlType::Paddle: case CtlType::Knob: s.norm = paramNorm(c.param); break;
        case CtlType::Rocker: s.on = v >= 0.5; break;
        case CtlType::Lever: s.step = static_cast<int>(std::lround(v)); break;
        case CtlType::ToneButton: {
            const int row = c.aux / 16, b = c.aux % 16;
            s.on = litTone_[row] == b;
            if ((b == 11 || b == 12) && plugin_) s.hasMemory = plugin_->hasMemory((b - 11) * 2 + row);
            break;
        }
        case CtlType::PresetLcd:
            if (plugin_) { s.text = plugin_->currentPresetName(); s.modified = plugin_->isPresetModified(); }
            if (!status_.empty() && std::chrono::steady_clock::now() < statusUntil_) { s.text = status_; s.modified = false; }
            break;
        case CtlType::Ribbon: s.ribbonTouch = ribbonTouch_; s.ribbonValue = v; break;
        case CtlType::Keyboard: s.keys = keysDown_; break;
        default: break;
    }
    return s;
}

void GuiWindow::addDirty(int x, int y, int w, int h) {
    const int x1 = std::min<int>(width_, x + w), y1 = std::min<int>(height_, y + h);
    x = std::max(0, x); y = std::max(0, y);
    if (x1 <= x || y1 <= y) return;
    if (dirtyX1_ <= dirtyX0_ || dirtyY1_ <= dirtyY0_) {
        dirtyX0_ = x; dirtyY0_ = y; dirtyX1_ = x1; dirtyY1_ = y1;
    } else {
        dirtyX0_ = std::min(dirtyX0_, x); dirtyY0_ = std::min(dirtyY0_, y);
        dirtyX1_ = std::max(dirtyX1_, x1); dirtyY1_ = std::max(dirtyY1_, y1);
    }
}

static void copyRect(std::vector<uint32_t>& dst, const std::vector<uint32_t>& src, int W, int H, int x, int y, int w, int h) {
    const int s = kScale;
    const int bw = W * s;
    const int x0 = std::max(0, x * s), y0 = std::max(0, y * s);
    const int x1 = std::min(W * s, (x + w) * s), y1 = std::min(H * s, (y + h) * s);
    if (x1 <= x0) return;
    for (int by = y0; by < y1; ++by) {
        std::memcpy(&dst[static_cast<size_t>(by) * bw + x0], &src[static_cast<size_t>(by) * bw + x0],
                    static_cast<size_t>(x1 - x0) * sizeof(uint32_t));
    }
}

void GuiWindow::composeControl(int i, const CtlState& st) {
    const Ctl& c = panelLayout().controls[static_cast<size_t>(i)];
    int mx, my;
    margins(c, mx, my);
    const int x = c.x - mx, y = c.y - my, w = c.w + 2 * mx, h = c.h + 2 * my;
    copyRect(compose_, static_, width_, height_, x, y, w, h);
    Graphics g(compose_.data(), width_, height_, kScale);
    panel::drawControl(g, c, st);
    addDirty(x, y, w, h);
}

void GuiWindow::downsample(int x0, int y0, int x1, int y1) {
    const int W = static_cast<int>(width_);
    const int bw = W * kScale;
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            const uint32_t* r0 = &display_[static_cast<size_t>(2 * py) * bw + 2 * px];
            const uint32_t* r1 = r0 + bw;
            const uint32_t p00 = r0[0], p01 = r0[1], p10 = r1[0], p11 = r1[1];
            const uint32_t r = (((p00 >> 16) & 0xFF) + ((p01 >> 16) & 0xFF) + ((p10 >> 16) & 0xFF) + ((p11 >> 16) & 0xFF) + 2) >> 2;
            const uint32_t gg = (((p00 >> 8) & 0xFF) + ((p01 >> 8) & 0xFF) + ((p10 >> 8) & 0xFF) + ((p11 >> 8) & 0xFF) + 2) >> 2;
            const uint32_t b = ((p00 & 0xFF) + (p01 & 0xFF) + (p10 & 0xFF) + (p11 & 0xFF) + 2) >> 2;
            pixels_[static_cast<size_t>(py) * W + px] = 0xFF000000u | (r << 16) | (gg << 8) | b;
        }
    }
}

void GuiWindow::updateReadout() {
    const auto& L = panelLayout();
    const int i = active_ >= 0 ? active_ : hover_;
    readout_.clear();
    if (i < 0 || !plugin_) return;
    const Ctl& c = L.controls[static_cast<size_t>(i)];
    if (c.param >= 0) {
        char val[64];
        plugin_->paramsValueToText(static_cast<clap_id>(c.param), plugin_->paramValue(static_cast<clap_id>(c.param)), val, sizeof(val));
        readout_ = std::string(paramInfo(static_cast<uint32_t>(c.param)).name) + ":  " + val;
        const char* desc = paramDescription(static_cast<uint32_t>(c.param));
        if (desc && *desc) readout_ += std::string("\n") + desc;
    } else if (c.type == CtlType::ToneButton) {
        const int row = c.aux / 16, b = c.aux % 16;
        if (b < 11) readout_ = std::string("LOAD FACTORY TONE INTO LINE ") + (row == 0 ? "I: " : "II: ") + kFactoryToneNames[row][b];
        else if (b < 13) readout_ = std::string("MEMORY ") + std::to_string((b - 11) * 2 + row + 1) + "  (SHIFT-CLICK STORES LINE " + (row == 0 ? "I)" : "II)");
        else readout_ = "PANEL: THE SLIDERS ARE ALWAYS LIVE";
    } else if (c.type == CtlType::PresetLcd) {
        readout_ = "CLICK FOR THE PRESET MENU, WHEEL TO STEP";
    } else if (c.type == CtlType::Keyboard) {
        readout_ = "CLICK TO PLAY (LOWER = HARDER)";
    }
    for (auto& ch : readout_) if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
}

void GuiWindow::renderFrame() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto& L = panelLayout();
    const size_t hiSize = static_cast<size_t>(width_) * kScale * height_ * kScale;
    if (static_.size() != hiSize) {
        static_.assign(hiSize, 0);
        compose_.assign(hiSize, 0);
        display_.assign(hiSize, 0);
        pixels_.assign(static_cast<size_t>(width_) * height_, 0xFF1B1C1E);
        staticValid_ = false;
    }
    if (!staticValid_) {
        Graphics g(static_.data(), width_, height_, kScale);
        panel::drawStatic(g, L);
        compose_ = static_;
        std::fill(drawnValid_.begin(), drawnValid_.end(), false);
        drawnReadout_ = "\x01";
        staticValid_ = true;
        addDirty(0, 0, static_cast<int>(width_), static_cast<int>(height_));
    }
    // Key states from the plugin (host notes and GUI clicks).
    bool keysChanged = false;
    if (plugin_) {
        for (int k = 0; k < 128; ++k) {
            const bool d = plugin_->isKeyDown(k);
            if (d != keysDown_[k]) { keysDown_[k] = d; keysChanged = true; }
        }
    }
    for (size_t i = 0; i < L.controls.size(); ++i) {
        const CtlState st = stateFor(static_cast<int>(i));
        const bool kb = L.controls[i].type == CtlType::Keyboard;
        if (!drawnValid_[i] || !sameState(st, drawn_[i]) || (kb && keysChanged)) {
            composeControl(static_cast<int>(i), st);
            drawn_[i] = st;
            drawnValid_[i] = true;
        }
    }
    // Overlays onto the display buffer.
    if (dirtyX1_ > dirtyX0_) copyRect(display_, compose_, width_, height_, dirtyX0_, dirtyY0_, dirtyX1_ - dirtyX0_, dirtyY1_ - dirtyY0_);
    updateReadout();
    if (readout_ != drawnReadout_ || menuOpen_ || menuWasOpen_) {
        int x, y, w, h;
        panel::readoutBounds(x, y, w, h);
        copyRect(display_, compose_, width_, height_, x, y, w, h);
        Graphics g(display_.data(), width_, height_, kScale);
        panel::drawReadout(g, readout_);
        drawnReadout_ = readout_;
        addDirty(x, y, w, h);
    }
    if (menuOpen_ || menuWasOpen_) {
        // Repaint under the menu as last drawn and as it is now (it may have
        // moved, changed submenu or closed), then the menu.
        for (int lv = 0; lv < 2; ++lv) {
            int r[4];
            menuBounds(lv, r[0], r[1], r[2], r[3]);
            const int* rects[2] = { prevMenu_[lv], r };
            for (const int* q : rects) {
                if (q[2] > 0) {
                    copyRect(display_, compose_, width_, height_, q[0] - 4, q[1] - 4, q[2] + 10, q[3] + 10);
                    addDirty(q[0] - 4, q[1] - 4, q[2] + 10, q[3] + 10);
                }
            }
            for (int k = 0; k < 4; ++k) prevMenu_[lv][k] = menuOpen_ ? r[k] : 0;
        }
        if (menuOpen_) {
            Graphics g(display_.data(), width_, height_, kScale);
            drawMenu(g);
        }
        menuWasOpen_ = menuOpen_;
    }
    if (dirtyX1_ > dirtyX0_ && dirtyY1_ > dirtyY0_) {
        downsample(dirtyX0_, dirtyY0_, dirtyX1_, dirtyY1_);
#if defined(__linux__) && !defined(__APPLE__)
        drawX11();
#elif defined(_WIN32)
        drawWin32();
#endif
    }
    dirtyX0_ = dirtyY0_ = dirtyX1_ = dirtyY1_ = 0;
}

// --- Interaction -------------------------------------------------------------------------------

int GuiWindow::controlIndexAt(int x, int y) const {
    const auto& L = panelLayout();
    for (size_t i = 0; i < L.controls.size(); ++i) {
        const Ctl& c = L.controls[i];
        int pad = (c.type == CtlType::Slider || c.type == CtlType::Paddle) ? 3 : 0;
        if (x >= c.x - pad && x < c.x + c.w + pad && y >= c.y - pad && y < c.y + c.h + pad) return static_cast<int>(i);
    }
    return -1;
}

void GuiWindow::setParamFromGui(int param, double value, bool gesture) {
    if (param < 0 || !plugin_) return;
    const clap_id id = static_cast<clap_id>(param);
    if (gesture) plugin_->onBeginEditFromGui(id);
    plugin_->onParamValueFromGui(id, clampParam(id, value));
    if (gesture) plugin_->onEndEditFromGui(id);
}

static bool isBlackKey(int k) { const int n = k % 12; return n == 1 || n == 3 || n == 6 || n == 8 || n == 10; }

int GuiWindow::keyAt(int x, int y) const {
    const auto& L = panelLayout();
    const Ctl* kb = nullptr;
    for (const auto& c : L.controls) if (c.type == CtlType::Keyboard) kb = &c;
    if (!kb || x < kb->x || x >= kb->x + kb->w || y < kb->y || y >= kb->y + kb->h) return -1;
    int whites = 0;
    for (int k = PanelLayout::kKeyboardFirst; k <= PanelLayout::kKeyboardLast; ++k) whites += isBlackKey(k) ? 0 : 1;
    const double ww = static_cast<double>(kb->w) / whites;
    // Black keys first (they sit on top).
    if (y < kb->y + kb->h * 0.62) {
        double xx = kb->x;
        for (int k = PanelLayout::kKeyboardFirst; k <= PanelLayout::kKeyboardLast; ++k) {
            if (!isBlackKey(k)) { xx += ww; continue; }
            if (std::fabs(x - xx) < ww * 0.31) return k;
        }
    }
    const int wi = static_cast<int>((x - kb->x) / ww);
    int n = 0;
    for (int k = PanelLayout::kKeyboardFirst; k <= PanelLayout::kKeyboardLast; ++k) {
        if (isBlackKey(k)) continue;
        if (n == wi) return k;
        ++n;
    }
    return -1;
}

void GuiWindow::beginEdit(int index) {
    const Ctl& c = panelLayout().controls[static_cast<size_t>(index)];
    active_ = index;
    if (c.param >= 0 && plugin_) {
        plugin_->onBeginEditFromGui(static_cast<clap_id>(c.param));
        dragStartValue_ = plugin_->paramValue(static_cast<clap_id>(c.param));
    }
}

void GuiWindow::handleMouseDown(int x, int y, bool shift) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    lastShift_ = shift;
    if (menuOpen_) {
        int lv = 1, hit = menuHit(1, x, y);
        if (hit < 0) { lv = 0; hit = menuHit(0, x, y); }
        if (hit < 0) { closeMenu(); renderFrame(); return; }
        const MenuItem& it = menu_[lv][static_cast<size_t>(hit)];
        if (it.action == kActCategory) { openSubmenu(hit); renderFrame(); return; }
        if (it.action == kActSeparator) return;
        closeMenu();
        if (plugin_) {
            if (it.action == kActInit) { plugin_->initPatch(); litTone_[0] = litTone_[1] = 13; }
            else if (it.action == kActSave) pendingFile_ = FileRequest::SavePreset;
            else if (it.action == kActRescan) { plugin_->library().scanUserPresets(); showStatus("USER PRESETS RESCANNED"); }
            else if (it.action >= 0) { plugin_->loadPreset(it.action); litTone_[0] = litTone_[1] = 13; }
        }
        renderFrame();
        return;
    }
    const int i = controlIndexAt(x, y);
    if (i < 0) return;
    const Ctl& c = panelLayout().controls[static_cast<size_t>(i)];
    const auto now = std::chrono::steady_clock::now();
    const bool dbl = (i == lastClick_ && now - lastClickTime_ < std::chrono::milliseconds(350));
    lastClick_ = i;
    lastClickTime_ = now;
    dragStartX_ = x;
    dragStartY_ = y;
    switch (c.type) {
        case CtlType::Slider: case CtlType::Paddle: case CtlType::Knob:
            if (dbl) {
                setParamFromGui(c.param, paramInfo(static_cast<uint32_t>(c.param)).def, true);
                lastClick_ = -1;
            } else {
                beginEdit(i);
            }
            break;
        case CtlType::Rocker:
            if (plugin_) setParamFromGui(c.param, plugin_->paramValue(static_cast<clap_id>(c.param)) >= 0.5 ? 0.0 : 1.0, true);
            break;
        case CtlType::Lever: {
            const int n = std::max(1, c.aux);
            const int step = std::min(n - 1, std::max(0, static_cast<int>((y - c.y - 4) / ((c.h - 8.0) / n))));
            setParamFromGui(c.param, step, true);
            break;
        }
        case CtlType::ToneButton: {
            const int row = c.aux / 16, b = c.aux % 16;
            if (!plugin_) break;
            if (b < 11) { plugin_->loadFactoryTone(row, b, row); litTone_[row] = b; }
            else if (b < 13) {
                const int slot = (b - 11) * 2 + row;
                if (shift) { plugin_->storeMemory(slot); showStatus("STORED MEMORY " + std::to_string(slot + 1)); }
                else if (plugin_->hasMemory(slot)) { plugin_->recallMemory(slot); litTone_[row] = b; }
                else showStatus("MEMORY EMPTY: SHIFT-CLICK TO STORE");
            } else {
                litTone_[row] = 13;
            }
            break;
        }
        case CtlType::PresetLcd: openMenu(c.x, c.y + c.h + 2); break;
        case CtlType::PresetPrev: active_ = i; if (plugin_) plugin_->stepPreset(-1); litTone_[0] = litTone_[1] = 13; break;
        case CtlType::PresetNext: active_ = i; if (plugin_) plugin_->stepPreset(1); litTone_[0] = litTone_[1] = 13; break;
        case CtlType::Ribbon:
            beginEdit(i);
            ribbonTouch_ = static_cast<double>(x - c.x) / c.w;
            if (plugin_) {
                // Touch first: the engine then bends the sounding notes relative to here (spec 03 §7).
                plugin_->onBeginEditFromGui(P_RIBBON_TOUCH);
                plugin_->onParamValueFromGui(P_RIBBON_TOUCH, 1.0);
                plugin_->onEndEditFromGui(P_RIBBON_TOUCH);
                plugin_->onParamValueFromGui(static_cast<clap_id>(c.param), 0.0);
            }
            break;
        case CtlType::Keyboard: {
            const int k = keyAt(x, y);
            if (k >= 0 && plugin_) {
                const double vel = 0.25 + 0.75 * std::min(1.0, std::max(0.0, (y - c.y) / static_cast<double>(c.h)));
                plugin_->guiNote(k, vel, true);
                keyboardKey_ = k;
                active_ = i;
            }
            break;
        }
    }
    renderFrame();
}

void GuiWindow::handleMouseDrag(int x, int y, bool shift) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (active_ < 0) return;
    const Ctl& c = panelLayout().controls[static_cast<size_t>(active_)];
    if (shift != lastShift_ && c.param >= 0 && plugin_) {
        dragStartX_ = x; dragStartY_ = y;
        dragStartValue_ = plugin_->paramValue(static_cast<clap_id>(c.param));
        lastShift_ = shift;
    }
    lastClick_ = -1;
    switch (c.type) {
        case CtlType::Slider: case CtlType::Paddle: case CtlType::Knob: {
            const ParamInfo& p = paramInfo(static_cast<uint32_t>(c.param));
            const double travel = (c.type == CtlType::Knob) ? 150.0 : (c.h - 14.0);
            double delta = (dragStartY_ - y) / travel;
            if (c.type == CtlType::Paddle) delta = -delta;   // paddles grow toward the player
            if (shift) delta *= 0.2;
            if (plugin_) plugin_->onParamValueFromGui(static_cast<clap_id>(c.param), clampParam(static_cast<uint32_t>(c.param), dragStartValue_ + delta * (p.max - p.min)));
            break;
        }
        case CtlType::Ribbon: {
            // Relative to the first touch: the full ribbon width is +-1 (one octave).
            const double v = 2.0 * (x - dragStartX_) / c.w;
            ribbonTouch_ = std::min(1.0, std::max(0.0, static_cast<double>(x - c.x) / c.w));
            if (plugin_) plugin_->onParamValueFromGui(static_cast<clap_id>(c.param), std::min(1.0, std::max(-1.0, v)));
            break;
        }
        case CtlType::Keyboard: {
            const int k = keyAt(x, y);
            if (k >= 0 && k != keyboardKey_ && plugin_) {
                if (keyboardKey_ >= 0) plugin_->guiNote(keyboardKey_, 0.0, false);
                const double vel = 0.25 + 0.75 * std::min(1.0, std::max(0.0, (y - c.y) / static_cast<double>(c.h)));
                plugin_->guiNote(k, vel, true);
                keyboardKey_ = k;
            }
            break;
        }
        default: break;
    }
    renderFrame();
}

void GuiWindow::handleMouseUp() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (active_ >= 0) {
        const Ctl& c = panelLayout().controls[static_cast<size_t>(active_)];
        if (c.type == CtlType::Ribbon && plugin_) {
            // Release the touch before the ribbon returns to 0, so held bends stay.
            plugin_->onBeginEditFromGui(P_RIBBON_TOUCH);
            plugin_->onParamValueFromGui(P_RIBBON_TOUCH, 0.0);
            plugin_->onEndEditFromGui(P_RIBBON_TOUCH);
            plugin_->onParamValueFromGui(static_cast<clap_id>(c.param), 0.0);
            ribbonTouch_ = -1.0;
        }
        if (c.type == CtlType::Keyboard && keyboardKey_ >= 0 && plugin_) {
            plugin_->guiNote(keyboardKey_, 0.0, false);
            keyboardKey_ = -1;
        }
        if (c.param >= 0 && plugin_ && (c.type == CtlType::Slider || c.type == CtlType::Paddle || c.type == CtlType::Knob || c.type == CtlType::Ribbon)) {
            plugin_->onEndEditFromGui(static_cast<clap_id>(c.param));
        }
    }
    active_ = -1;
    renderFrame();
}

void GuiWindow::handleMouseMove(int x, int y) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (menuOpen_) {
        const int h1 = menuHit(1, x, y);
        const int h0 = h1 >= 0 ? -1 : menuHit(0, x, y);
        bool changed = h1 != menuHover_[1] || h0 != menuHover_[0];
        menuHover_[1] = h1;
        menuHover_[0] = h0 >= 0 ? h0 : (h1 >= 0 ? menuHover_[0] : -1);
        if (h0 >= 0 && menu_[0][static_cast<size_t>(h0)].action == kActCategory && openCategory_ != h0) {
            openSubmenu(h0);
            changed = true;
        }
        if (changed) renderFrame();
        return;
    }
    const int h = controlIndexAt(x, y);
    if (h != hover_) {
        hover_ = h;
        renderFrame();
    }
}

void GuiWindow::handleWheel(int x, int y, int delta) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (menuOpen_ || delta == 0) return;
    const int i = controlIndexAt(x, y);
    if (i < 0 || !plugin_) return;
    const Ctl& c = panelLayout().controls[static_cast<size_t>(i)];
    if (c.type == CtlType::PresetLcd) { plugin_->stepPreset(delta > 0 ? -1 : 1); renderFrame(); return; }
    if (c.param < 0 || c.type == CtlType::Ribbon) return;
    const ParamInfo& p = paramInfo(static_cast<uint32_t>(c.param));
    double v = plugin_->paramValue(static_cast<clap_id>(c.param));
    double step = (p.flags & PF_STEPPED) ? 1.0 : 0.02 * (p.max - p.min);
    if (c.type == CtlType::Paddle) step = -step;
    if (c.type == CtlType::Lever) step = -step;   // wheel up moves the lever up
    v += delta > 0 ? step : -step;
    setParamFromGui(c.param, v, true);
    renderFrame();
}

void GuiWindow::handleKey(int key) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (key == 27 && menuOpen_) { closeMenu(); renderFrame(); }
}

void GuiWindow::showStatus(const std::string& text) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    status_ = text;
    statusUntil_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(2500);
}

// --- Menu ----------------------------------------------------------------------------------------

void GuiWindow::openMenu(int x, int y) {
    menu_[0].clear();
    menu_[1].clear();
    openCategory_ = -1;
    menu_[0].push_back({ "INIT PATCH", kActInit, "" });
    menu_[0].push_back({ "SAVE PRESET AS...", kActSave, "" });
    menu_[0].push_back({ "RESCAN USER PRESETS", kActRescan, "" });
    menu_[0].push_back({ "", kActSeparator, "" });
    if (plugin_) {
        for (const auto& cat : plugin_->library().categories()) {
            menu_[0].push_back({ (cat == "USER" ? std::string("   ") : cat + " ") + PresetLibrary::categoryName(cat) + "  >", kActCategory, cat });
        }
    }
    int w = 0;
    for (const auto& it : menu_[0]) w = std::max(w, static_cast<int>(modern::textWidth(modern::kLabelFont, it.label.c_str(), 6.f)));
    menuW_[0] = w + 22;
    menuCols_[0] = 1;
    menuRows_[0] = static_cast<int>(menu_[0].size());
    menuX_[0] = std::max(2, std::min(x, static_cast<int>(width_) - menuW_[0] - 4));
    menuY_[0] = std::max(2, std::min(y, static_cast<int>(height_) - menuRows_[0] * kItemH - 10));
    menuHover_[0] = menuHover_[1] = -1;
    menuOpen_ = true;
}

void GuiWindow::openSubmenu(int item) {
    menu_[1].clear();
    openCategory_ = item;
    if (!plugin_ || item < 0) return;
    const std::string cat = menu_[0][static_cast<size_t>(item)].category;
    const auto& presets = plugin_->library().presets();
    const int current = plugin_->currentPresetIndex();
    for (size_t i = 0; i < presets.size(); ++i) {
        const auto& p = presets[i];
        const bool match = (cat == "USER") ? p.user : (!p.user && p.category == cat);
        if (match) menu_[1].push_back({ (static_cast<int>(i) == current ? "> " : "  ") + p.name, static_cast<int>(i), "" });
    }
    int w = 0;
    for (const auto& it : menu_[1]) w = std::max(w, static_cast<int>(modern::textWidth(modern::kLabelFont, it.label.c_str(), 6.f)));
    menuW_[1] = w + 18;
    const int maxRows = (static_cast<int>(height_) - 20) / kItemH;
    const int n = std::max(1, static_cast<int>(menu_[1].size()));
    menuCols_[1] = (n + maxRows - 1) / maxRows;
    menuRows_[1] = (n + menuCols_[1] - 1) / menuCols_[1];
    const int totalW = menuW_[1] * menuCols_[1];
    int x = menuX_[0] + menuW_[0] + 2;
    if (x + totalW > static_cast<int>(width_) - 2) x = std::max(2, menuX_[0] - totalW - 2);
    int y = menuY_[0] + 3 + item * kItemH;
    y = std::max(2, std::min(y, static_cast<int>(height_) - menuRows_[1] * kItemH - 10));
    menuX_[1] = x;
    menuY_[1] = y;
    menuHover_[1] = -1;
}

void GuiWindow::closeMenu() {
    menuOpen_ = false;
    openCategory_ = -1;
}

void GuiWindow::menuBounds(int lv, int& x, int& y, int& w, int& h) const {
    if (menu_[lv].empty()) { x = y = w = h = 0; return; }
    x = menuX_[lv]; y = menuY_[lv];
    w = menuW_[lv] * menuCols_[lv];
    h = menuRows_[lv] * kItemH + 6;
}

void GuiWindow::itemRect(int lv, int i, int& x, int& y, int& w, int& h) const {
    const int col = i / std::max(1, menuRows_[lv]), row = i % std::max(1, menuRows_[lv]);
    x = menuX_[lv] + col * menuW_[lv];
    y = menuY_[lv] + 3 + row * kItemH;
    w = menuW_[lv];
    h = kItemH;
}

int GuiWindow::menuHit(int lv, int x, int y) const {
    if (!menuOpen_) return -1;
    for (int i = 0; i < static_cast<int>(menu_[lv].size()); ++i) {
        int ix, iy, iw, ih;
        itemRect(lv, i, ix, iy, iw, ih);
        if (x >= ix && x < ix + iw && y >= iy && y < iy + ih) return i;
    }
    return -1;
}

int GuiWindow::menuItemCount(int lv) const { return (lv == 0 || lv == 1) ? static_cast<int>(menu_[lv].size()) : 0; }
std::string GuiWindow::menuItemLabel(int lv, int i) const {
    return (i >= 0 && i < menuItemCount(lv)) ? menu_[lv][static_cast<size_t>(i)].label : std::string();
}
bool GuiWindow::menuItemCenter(int lv, int i, int& x, int& y) const {
    if (i < 0 || i >= menuItemCount(lv)) return false;
    int ix, iy, iw, ih;
    itemRect(lv, i, ix, iy, iw, ih);
    x = ix + iw / 2; y = iy + ih / 2;
    return true;
}

void GuiWindow::drawMenu(Graphics& g) {
    using namespace modern;
    for (int lv = 0; lv < 2; ++lv) {
        if (menu_[lv].empty()) continue;
        int x, y, w, h;
        menuBounds(lv, x, y, w, h);
        shadeBox(g, x + 3.f, y + 4.f, x + w + 3.f, y + h + 4.f, [](float, float) { return 0x60000000u; });
        shadeBox(g, static_cast<float>(x), static_cast<float>(y), static_cast<float>(x + w), static_cast<float>(y + h),
                 [&](float xx, float yy) {
                     const bool edge = xx < x + 1 || xx >= x + w - 1 || yy < y + 1 || yy >= y + h - 1;
                     return edge ? 0xFF4A4E52u : 0xF5101214u;
                 });
        for (int i = 0; i < static_cast<int>(menu_[lv].size()); ++i) {
            const MenuItem& it = menu_[lv][static_cast<size_t>(i)];
            int ix, iy, iw, ih;
            itemRect(lv, i, ix, iy, iw, ih);
            if (it.action == kActSeparator) {
                drawLineAA(g, ix + 4.f, iy + ih * 0.5f, ix + iw - 4.f, iy + ih * 0.5f, 1.f, 0xFF2C3030);
                continue;
            }
            const bool hot = (menuHover_[lv] == i) || (lv == 0 && openCategory_ == i);
            if (hot) shadeBox(g, ix + 2.f, static_cast<float>(iy), ix + iw - 2.f, static_cast<float>(iy + ih), [](float, float) { return 0xFF1B8224u; });
            drawText(g, kLabelFont, it.label.c_str(), ix + 8.f, iy + 3.5f, 6.f, hot ? 0xFF000000 : 0xFF45E86A);
        }
    }
}

GuiWindow::FileRequest GuiWindow::takeFileRequest() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const FileRequest r = pendingFile_;
    pendingFile_ = FileRequest::NoRequest;
    return r;
}

FileDialogOptions GuiWindow::fileDialogOptions() const {
    FileDialogOptions o;
    o.save = true;
    o.title = "Save Tannhauser preset";
    o.filterName = "Tannhauser presets";
    o.extension = "tpreset";
    std::string dir = PresetLibrary::userPresetDirectory();
    std::error_code ec;
    if (!dir.empty()) std::filesystem::create_directories(std::filesystem::u8path(dir), ec);
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') dir += '/';
    std::string name = plugin_ ? plugin_->currentPresetName() : "My Preset";
    for (char& ch : name) if (ch == '/' || ch == '\\' || ch == ':') ch = '-';
    o.startPath = dir + name + ".tpreset";
    return o;
}

void GuiWindow::finishFileRequest(const std::string& path) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (path.empty() || !plugin_) return;
    std::string error;
    showStatus(plugin_->saveUserPreset(path, error) ? "PRESET SAVED" : "SAVE FAILED");
    if (!error.empty()) std::fprintf(stderr, "Tannhauser: %s\n", error.c_str());
    renderFrame();
}

// --- Window plumbing -------------------------------------------------------------------------------

bool GuiWindow::setParent(const clap_window_t* window) {
    if (!window || !window->api) return false;
#if defined(__linux__) && !defined(__APPLE__)
    if (std::strcmp(window->api, CLAP_WINDOW_API_X11) == 0) {
        parent11_ = window->x11;
        initX11();
        return true;
    }
#elif defined(_WIN32)
    if (std::strcmp(window->api, CLAP_WINDOW_API_WIN32) == 0) {
        parentHwnd_ = window->win32;
        initWin32();
        return true;
    }
#endif
    return false;
}

bool GuiWindow::setSize(uint32_t width, uint32_t height) {
    // Fixed layout (spec 06 §2): only the native size is accepted.
    return width == kDefaultWidth && height == kDefaultHeight;
}

bool GuiWindow::show() { visible_ = true; renderFrame(); return true; }
bool GuiWindow::hide() { visible_ = false; return true; }

void GuiWindow::destroy() {
    running_ = false;
    if (eventThread_.joinable()) eventThread_.join();
#if defined(__linux__) && !defined(__APPLE__)
    if (display11_ && created11_) {
        Display* d = static_cast<Display*>(display11_);
        XDestroyWindow(d, window11_);
        XCloseDisplay(d);
        display11_ = nullptr;
        created11_ = false;
    }
#elif defined(_WIN32)
    if (hwnd_) {
        DestroyWindow(static_cast<HWND>(hwnd_));
        hwnd_ = nullptr;
    }
#endif
}

#if defined(__linux__) && !defined(__APPLE__)
void GuiWindow::initX11() {
    if (created11_) return;
    Display* d = XOpenDisplay(nullptr);
    if (!d) return;
    display11_ = d;
    const int screen = DefaultScreen(d);
    const Window parent = parent11_ ? parent11_ : RootWindow(d, screen);
    window11_ = XCreateSimpleWindow(d, parent, 0, 0, width_, height_, 0, BlackPixel(d, screen), BlackPixel(d, screen));
    XSelectInput(d, window11_, ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | KeyPressMask);
    XMapWindow(d, window11_);
    XFlush(d);
    created11_ = true;
    renderFrame();
    running_ = true;
    eventThread_ = std::thread(&GuiWindow::eventLoopX11, this);
}

void GuiWindow::eventLoopX11() {
    try {
        Display* d = static_cast<Display*>(display11_);
        auto lastPaint = std::chrono::steady_clock::now();
        while (running_) {
            while (XPending(d) > 0) {
                XEvent ev;
                XNextEvent(d, &ev);
                if (ev.type == Expose) {
                    std::lock_guard<std::recursive_mutex> lock(mutex_);
                    addDirty(0, 0, static_cast<int>(width_), static_cast<int>(height_));
                    drawX11();
                    dirtyX0_ = dirtyY0_ = dirtyX1_ = dirtyY1_ = 0;
                } else if (ev.type == ButtonPress) {
                    const bool shift = (ev.xbutton.state & ShiftMask) != 0;
                    if (ev.xbutton.button == Button1) handleMouseDown(ev.xbutton.x, ev.xbutton.y, shift);
                    else if (ev.xbutton.button == Button3) handleKey(27);
                    else if (ev.xbutton.button == Button4) handleWheel(ev.xbutton.x, ev.xbutton.y, 1);
                    else if (ev.xbutton.button == Button5) handleWheel(ev.xbutton.x, ev.xbutton.y, -1);
                } else if (ev.type == ButtonRelease && ev.xbutton.button == Button1) {
                    handleMouseUp();
                } else if (ev.type == MotionNotify) {
                    if (ev.xmotion.state & Button1Mask) handleMouseDrag(ev.xmotion.x, ev.xmotion.y, (ev.xmotion.state & ShiftMask) != 0);
                    else handleMouseMove(ev.xmotion.x, ev.xmotion.y);
                } else if (ev.type == KeyPress) {
                    if (XLookupKeysym(&ev.xkey, 0) == XK_Escape) handleKey(27);
                }
            }
            if (takeFileRequest() == FileRequest::SavePreset && !fileDialog_.running()) {
                if (!fileDialog_.start(fileDialogOptions())) showStatus("NO FILE DIALOG (ZENITY/KDIALOG)");
            }
            std::string chosen;
            bool save = false;
            if (fileDialog_.poll(chosen, save)) finishFileRequest(chosen);
            const auto now = std::chrono::steady_clock::now();
            if (visible_ && now - lastPaint >= std::chrono::milliseconds(33)) {
                lastPaint = now;
                renderFrame();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    } catch (...) {
        // Never let an exception escape into the host.
    }
}

void GuiWindow::drawX11() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!display11_ || !created11_) return;
    const int W = static_cast<int>(width_), H = static_cast<int>(height_);
    const int x0 = std::max(0, dirtyX0_), y0 = std::max(0, dirtyY0_);
    const int x1 = std::min(W, dirtyX1_), y1 = std::min(H, dirtyY1_);
    if (x1 <= x0 || y1 <= y0) return;
    Display* d = static_cast<Display*>(display11_);
    const int screen = DefaultScreen(d);
    XImage* img = XCreateImage(d, DefaultVisual(d, screen), 24, ZPixmap, 0, reinterpret_cast<char*>(pixels_.data()),
                               width_, height_, 32, 0);
    if (!img) return;
    XPutImage(d, window11_, DefaultGC(d, screen), img, x0, y0, x0, y0, static_cast<unsigned>(x1 - x0), static_cast<unsigned>(y1 - y0));
    img->data = nullptr;
    XDestroyImage(img);
    XFlush(d);
}
#endif

#if defined(_WIN32)
static const wchar_t* kClassName = L"TannhauserWindowClass";
static bool g_classRegistered = false;

static LRESULT CALLBACK wndProcImpl(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    GuiWindow* gui = reinterpret_cast<GuiWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            SetTimer(hwnd, 1, 33, nullptr);
            return 0;
        }
        case WM_TIMER: if (gui && gui->isVisible()) gui->renderFrame(); return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            if (gui) gui->drawWin32();
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN:
            if (gui) {
                SetCapture(hwnd);
                SetFocus(hwnd);
                gui->handleMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (wParam & MK_SHIFT) != 0);
                if (gui->takeFileRequest() == GuiWindow::FileRequest::SavePreset) {
                    ReleaseCapture();
                    const std::filesystem::path file = runFileDialog(hwnd, gui->fileDialogOptions());
                    gui->finishFileRequest(file.u8string());
                }
            }
            return 0;
        case WM_RBUTTONDOWN: if (gui) gui->handleKey(27); return 0;
        case WM_MOUSEMOVE:
            if (gui) {
                if (wParam & MK_LBUTTON) gui->handleMouseDrag(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (wParam & MK_SHIFT) != 0);
                else gui->handleMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            }
            return 0;
        case WM_LBUTTONUP: if (gui) { ReleaseCapture(); gui->handleMouseUp(); } return 0;
        case WM_MOUSEWHEEL:
            if (gui) {
                POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ScreenToClient(hwnd, &pt);
                gui->handleWheel(pt.x, pt.y, GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 1 : -1);
            }
            return 0;
        case WM_KEYDOWN: if (gui && wParam == VK_ESCAPE) gui->handleKey(27); return 0;
        case WM_DESTROY: KillTimer(hwnd, 1); return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    try { return wndProcImpl(hwnd, msg, wParam, lParam); } catch (...) { return DefWindowProcW(hwnd, msg, wParam, lParam); }
}

void GuiWindow::initWin32() {
    if (hwnd_) return;
    HINSTANCE inst = GetModuleHandleW(nullptr);
    if (!g_classRegistered) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = wndProc;
        wc.hInstance = inst;
        wc.lpszClassName = kClassName;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
        g_classRegistered = true;
    }
    hwnd_ = CreateWindowExW(0, kClassName, L"Tannhauser", WS_CHILD | WS_VISIBLE, 0, 0, width_, height_,
                            static_cast<HWND>(parentHwnd_), nullptr, inst, this);
    renderFrame();
}

void GuiWindow::drawWin32() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!hwnd_) return;
    HDC hdc = GetDC(static_cast<HWND>(hwnd_));
    if (!hdc) return;
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(width_);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(height_);
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(hdc, 0, 0, width_, height_, 0, 0, 0, height_, pixels_.data(), &bmi, DIB_RGB_COLORS);
    ReleaseDC(static_cast<HWND>(hwnd_), hdc);
}
#endif

// --- CLAP GUI extension ----------------------------------------------------------------------------

static TannhauserClap* selfOf(const clap_plugin_t* p) { return static_cast<TannhauserClap*>(p->plugin_data); }

const clap_plugin_gui_t g_tannhauserGuiExtension = {
    [](const clap_plugin_t*, const char* api, bool floating) -> bool {
#if defined(__linux__) && !defined(__APPLE__)
        return api && std::strcmp(api, CLAP_WINDOW_API_X11) == 0 && !floating;
#elif defined(_WIN32)
        return api && std::strcmp(api, CLAP_WINDOW_API_WIN32) == 0 && !floating;
#else
        (void)api; (void)floating;
        return false;
#endif
    },
    [](const clap_plugin_t*, const char** api, bool* floating) -> bool {
        if (!api || !floating) return false;
#if defined(__linux__) && !defined(__APPLE__)
        *api = CLAP_WINDOW_API_X11;
#elif defined(_WIN32)
        *api = CLAP_WINDOW_API_WIN32;
#else
        return false;
#endif
        *floating = false;
        return true;
    },
    [](const clap_plugin_t* p, const char*, bool) -> bool {
        try { selfOf(p)->createGuiWindow(); } catch (...) { return false; }
        return true;
    },
    [](const clap_plugin_t* p) { try { selfOf(p)->destroyGuiWindow(); } catch (...) {} },
    [](const clap_plugin_t*, double) -> bool { return false; },
    [](const clap_plugin_t*, uint32_t* w, uint32_t* h) -> bool {
        if (!w || !h) return false;
        *w = GuiWindow::kDefaultWidth;
        *h = GuiWindow::kDefaultHeight;
        return true;
    },
    [](const clap_plugin_t*) -> bool { return false; },
    [](const clap_plugin_t*, clap_gui_resize_hints_t*) -> bool { return false; },
    [](const clap_plugin_t*, uint32_t* w, uint32_t* h) -> bool {
        if (!w || !h) return false;
        *w = GuiWindow::kDefaultWidth;
        *h = GuiWindow::kDefaultHeight;
        return true;
    },
    [](const clap_plugin_t* p, uint32_t w, uint32_t h) -> bool {
        try { auto* g = selfOf(p)->getGuiWindow(); return g ? g->setSize(w, h) : true; } catch (...) { return false; }
    },
    [](const clap_plugin_t* p, const clap_window_t* window) -> bool {
        if (!window || !window->api) return false;
        try {
            if (!selfOf(p)->getGuiWindow()) selfOf(p)->createGuiWindow();
            return selfOf(p)->getGuiWindow()->setParent(window);
        } catch (...) { return false; }
    },
    [](const clap_plugin_t*, const clap_window_t*) -> bool { return false; },
    [](const clap_plugin_t*, const char*) {},
    [](const clap_plugin_t* p) -> bool {
        try { auto* g = selfOf(p)->getGuiWindow(); return g ? g->show() : false; } catch (...) { return false; }
    },
    [](const clap_plugin_t* p) -> bool {
        try { auto* g = selfOf(p)->getGuiWindow(); return g ? g->hide() : false; } catch (...) { return false; }
    },
};

} // namespace tannhauser
