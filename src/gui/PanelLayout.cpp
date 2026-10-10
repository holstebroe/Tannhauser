#include "PanelLayout.hpp"
#include "core/Params.hpp"
#include <initializer_list>
#include <utility>

namespace tannhauser {

namespace {

const char* const kFeetLabels[] = { "16'", "8'", "5 1/3'", "4'", "2 2/3'", "2'" };
const char* const kSubLabels[] = { "SINE", "SAW UP", "SAW DN", "SQUARE", "S / H", "NOISE" };

// Programming-row columns: name and scale legends as printed on the CS-80
// (photos of the hardware panel, spec 06 §2).
struct Col { LineParam p; CtlType t; const char* label; uint32_t cap; const char* top; const char* bottom; };

const Col kVco[] = {
    { LP_PWM_SPEED, CtlType::Slider, "SPEED", kCapWhite, "FAST", "SLOW" },
    { LP_PWM_DEPTH, CtlType::Slider, "PWM", kCapGreen, "10", "0" },
    { LP_PW, CtlType::Slider, "PW", kCapGreen, "90%", "50%" },
    { LP_SQUARE, CtlType::Rocker, "~SQ", kCapBlack, "ON", nullptr },
    { LP_SAW, CtlType::Rocker, "~SAW", kCapBlack, "ON", nullptr },
    { LP_NOISE, CtlType::Slider, "NOISE", kCapWhite, "10", "0" },
};
const Col kVcf[] = {
    { LP_HPF, CtlType::Slider, "HPF", kCapGreen, "HIGH", "LOW" },
    { LP_RES_H, CtlType::Slider, "RES_H", kCapRed, "HIGH", "LOW" },
    { LP_LPF, CtlType::Slider, "LPF", kCapGreen, "HIGH", "LOW" },
    { LP_RES_L, CtlType::Slider, "RES_L", kCapRed, "HIGH", "LOW" },
    { LP_IL, CtlType::Slider, "IL", kCapBlack, "-5", "0" },
    { LP_AL, CtlType::Slider, "AL", kCapBlack, "+5", "0" },
    { LP_FEG_A, CtlType::Slider, "A", kCapBlack, "LONG", "SHORT" },
    { LP_FEG_D, CtlType::Slider, "D", kCapBlack, "LONG", "SHORT" },
    { LP_FEG_R, CtlType::Slider, "R", kCapYellow, "LONG", "SHORT" },
};
const Col kVca[] = {
    { LP_VCF_LEVEL, CtlType::Slider, "VCF LEVEL", kCapWhite, "10", "0" },
    { LP_SINE, CtlType::Slider, "~SINE", kCapWhite, "10", "0" },
    { LP_VEG_A, CtlType::Slider, "A", kCapBlack, "LONG", "SHORT" },
    { LP_VEG_D, CtlType::Slider, "D", kCapBlack, "LONG", "SHORT" },
    { LP_VEG_S, CtlType::Slider, "S", kCapBlack, "10", "0" },
    { LP_VEG_R, CtlType::Slider, "R", kCapYellow, "LONG", "SHORT" },
    { LP_LEVEL, CtlType::Slider, "LEVEL", kCapWhite, "10", "0" },
};
const Col kInit[] = {
    { LP_INIT_BRILL, CtlType::Slider, "BRILL.", kCapGreen, "10", "0" },
    { LP_INIT_LEVEL, CtlType::Slider, "LEVEL", kCapWhite, "10", "0" },
};
const Col kAfter[] = {
    { LP_AFTER_BRILL, CtlType::Slider, "BRILL.", kCapGreen, "10", "0" },
    { LP_AFTER_LEVEL, CtlType::Slider, "LEVEL", kCapWhite, "10", "0" },
};

constexpr int kColW = 44;

PanelLayout build() {
    PanelLayout L;
    L.rowTop[0] = 40;
    L.rowTop[1] = 172;
    for (int row = 0; row < 2; ++row) {
        const int ry = L.rowTop[row];
        L.controls.push_back({ CtlType::Lever, static_cast<int>(lineParam(row, LP_FEET)), 50, ry + 22, 50, 100,
                               "FEET", kCapWhite, 6, kFeetLabels });
        int x = 108;
        auto group = [&](const Col* cols, int n, const char* title) {
            L.frames.push_back({ x - 4, ry + 2, n * kColW + 8, 124, title });
            for (int i = 0; i < n; ++i) {
                const int cx = x + i * kColW + kColW / 2;
                const Col& c = cols[i];
                const int id = static_cast<int>(lineParam(row, c.p));
                // Names under the controls, scale legends above/below the slot (as printed on the CS-80).
                if (c.t == CtlType::Rocker) L.controls.push_back({ c.t, id, cx - 10, ry + 44, 20, 40, c.label, c.cap, 0, nullptr, c.top, c.bottom, ry + 112 });
                else L.controls.push_back({ c.t, id, cx - 12, ry + 26, 24, 78, c.label, c.cap, 0, nullptr, c.top, c.bottom, ry + 112 });
            }
            x += n * kColW + 12;
        };
        group(kVco, 6, "VCO");
        group(kVcf, 9, "VCF");
        group(kVca, 7, "VCA");
        group(kInit, 2, "INITIAL");
        group(kAfter, 2, "AFTER");
    }

    // Middle strip: performance paddles, tone selector, knobs.
    const int mt = 306;
    L.middleTop = mt;
    L.middleH = 152;
    // Paddle groups; captions printed under the paddles (bottom legends) need the 8 px
    // left free below them.
    struct PaddleItem { int param; const char* label; const char* top; const char* bottom; };
    auto paddleGroup = [&](int x0, const char* title, std::initializer_list<PaddleItem> items,
                           uint32_t cap, int extraLeft, int top, int h) {
        const int n = static_cast<int>(items.size());
        L.frames.push_back({ x0, top + 2, extraLeft + n * 34 + 6, h - 6, title });
        int i = 0;
        for (const auto& it : items) {
            const int cx = x0 + extraLeft + 3 + i * 34 + 17;
            L.controls.push_back({ CtlType::Paddle, it.param, cx - 11, top + 30, 22, h - 48, it.label, cap, 0, nullptr,
                                   it.top, it.bottom, 0 });
            ++i;
        }
    };
    auto paddleX = [](int x0, int extraLeft, int i) { return static_cast<float>(x0 + extraLeft + 3 + i * 34 + 17); };
    // Volume.
    L.frames.push_back({ 22, mt + 2, 60, 146, "VOLUME" });
    L.controls.push_back({ CtlType::Knob, P_VOLUME, 30, mt + 50, 44, 44, "", kCapBlack, 0, nullptr });
    // Sub oscillator with its function lever.
    L.controls.push_back({ CtlType::Lever, P_SUB_FUNC, 92, mt + 28, 58, 112, "FUNCTION", kCapWhite, 6, kSubLabels });
    paddleGroup(86, "SUB OSCILLATOR", { { P_SUB_SPEED, "SPEED", nullptr, nullptr }, { P_SUB_VCO, "VCO", nullptr, nullptr },
                                         { P_SUB_VCF, "VCF", nullptr, nullptr }, { P_SUB_VCA, "VCA", nullptr, nullptr } },
                kCapGrey, 66, mt, 152);
    paddleGroup(300, "TOUCH RESPONSE", { { P_TOUCH_BEND, "P.BEND", nullptr, "INITIAL" }, { P_TOUCH_SPEED, "SPEED", nullptr, nullptr },
                                         { P_TOUCH_VCO, "VCO", nullptr, nullptr }, { P_TOUCH_VCF, "VCF", nullptr, nullptr } },
                kCapGrey, 0, mt, 152);
    L.captions.push_back({ paddleX(300, 0, 2), mt + 137.f, "SUB-OSC AFTER", 4.5f });
    // Tone selector and the preset LCD.
    L.frames.push_back({ 446, mt + 2, 344, 146, "TONE SELECTOR" });
    L.controls.push_back({ CtlType::PresetPrev, -1, 452, mt + 20, 22, 26, "<", 0, 0, nullptr });
    L.controls.push_back({ CtlType::PresetLcd, -1, 478, mt + 20, 284, 26, "", 0, 0, nullptr });
    L.controls.push_back({ CtlType::PresetNext, -1, 766, mt + 20, 22, 26, ">", 0, 0, nullptr });
    for (int row = 0; row < 2; ++row) {
        for (int b = 0; b < 14; ++b) {
            L.controls.push_back({ CtlType::ToneButton, -1, 451 + b * 24, mt + 54 + row * 46, 22, 40, "", 0,
                                   row * 16 + b, nullptr });
        }
    }
    // Keyboard control: BRILLIANCE and LEVEL over LOW/HIGH pairs, as on the CS-80.
    paddleGroup(798, "KEYBOARD CONTROL", { { P_KBD_BRILL_LOW, "", nullptr, "LOW" }, { P_KBD_BRILL_HIGH, "", nullptr, "HIGH" },
                                            { P_KBD_LEVEL_LOW, "", nullptr, "LOW" }, { P_KBD_LEVEL_HIGH, "", nullptr, "HIGH" } },
                kCapGreen, 0, mt, 152);
    L.captions.push_back({ 0.5f * (paddleX(798, 0, 0) + paddleX(798, 0, 1)), mt + 17.f, "BRILLIANCE", 6.f });
    L.captions.push_back({ 0.5f * (paddleX(798, 0, 2) + paddleX(798, 0, 3)), mt + 17.f, "LEVEL", 6.f });
    paddleGroup(944, "RING MODULATOR", { { P_RM_ATTACK, "ATTACK", nullptr, nullptr }, { P_RM_DECAY, "DECAY", nullptr, nullptr },
                                          { P_RM_DEPTH, "DEPTH", nullptr, nullptr }, { P_RM_SPEED, "SPEED", nullptr, nullptr },
                                          { P_RM_MOD, "MOD.", nullptr, nullptr } },
                kCapGrey, 0, mt, 152);
    paddleGroup(1124, "", { { P_DETUNE, "DETUNE", nullptr, nullptr }, { P_MIX, "MIX", "I", "II" },
                            { P_BRILLIANCE, "BRILL.", nullptr, nullptr }, { P_RESONANCE, "RES.", nullptr, nullptr } },
                kCapWhite, 0, mt, 152);
    L.frames.push_back({ 1272, mt + 2, 66, 146, "PITCH" });
    L.controls.push_back({ CtlType::Knob, P_PITCH, 1283, mt + 50, 44, 44, "", kCapBlack, 0, nullptr });

    // Bottom: left-hand panel, ribbon and keyboard.
    const int bt = 462;
    L.bottomTop = bt;
    L.frames.push_back({ 22, bt + 2, 122, 146, "SUSTAIN" });
    // Left-hand panel as on the CS-80: the time controls are sliders, LONG at the top.
    L.controls.push_back({ CtlType::Rocker, P_SUS_MODE, 32, bt + 52, 20, 40, "", kCapBlack, 0, nullptr, "I", "II", 0 });
    L.controls.push_back({ CtlType::Rocker, P_SUS_PEDAL, 68, bt + 52, 20, 40, "PEDAL", kCapBlack, 0, nullptr });
    L.controls.push_back({ CtlType::Slider, P_SUS_TIME, 102, bt + 30, 24, 104, "TIME", kCapYellow, 0, nullptr, "LONG", "SHORT", 0 });
    L.frames.push_back({ 150, bt + 2, 84, 146, "PORTA/GLISS" });
    L.controls.push_back({ CtlType::Rocker, P_PORTA_MODE, 160, bt + 52, 20, 40, "", kCapBlack, 0, nullptr, "PORTA.", "GLISS.", 0 });
    L.controls.push_back({ CtlType::Slider, P_PORTA_TIME, 195, bt + 30, 24, 104, "TIME", kCapWhite, 0, nullptr, "LONG", "SHORT", 0 });
    L.frames.push_back({ 240, bt + 2, 150, 146, "CHORUS / TREMOLO" });
    L.controls.push_back({ CtlType::Rocker, P_CHORUS, 250, bt + 52, 20, 40, "CHORUS", kCapBlack, 0, nullptr });
    L.controls.push_back({ CtlType::Rocker, P_TREMOLO, 286, bt + 52, 20, 40, "TREM.", kCapBlack, 0, nullptr });
    L.controls.push_back({ CtlType::Paddle, P_FX_SPEED, 322, bt + 30, 22, 106, "SPEED", kCapGrey, 0, nullptr });
    L.controls.push_back({ CtlType::Paddle, P_FX_DEPTH, 358, bt + 30, 22, 106, "DEPTH", kCapGrey, 0, nullptr });
    L.frames.push_back({ 396, bt + 2, 212, 146, "REVERB / EXTRA" });
    const std::pair<int, const char*> extras[] = { { P_REV_MIX, "MIX" }, { P_REV_DECAY, "DECAY" }, { P_REV_TONE, "TONE" },
                                                   { P_REV_PREDELAY, "PRE" }, { P_DRIFT, "DRIFT" } };
    for (int i = 0; i < 5; ++i) {
        L.controls.push_back({ CtlType::Paddle, extras[i].first, 404 + i * 33, bt + 30, 22, 106, extras[i].second,
                               i == 4 ? kCapWhite : kCapBlue, 0, nullptr });
    }
    // Long envelope mode [A] (spec 02 §5/§6).
    L.controls.push_back({ CtlType::Rocker, P_ENV_LONG, 574, bt + 52, 20, 40, "LONG", kCapBlack, 0, nullptr });
    L.controls.push_back({ CtlType::Ribbon, P_RIBBON, 620, bt + 6, 716, 20, "RIBBON", 0, 0, nullptr });
    L.controls.push_back({ CtlType::Keyboard, -1, 620, bt + 32, 716, 118, "", 0, 0, nullptr });
    return L;
}

} // namespace

const PanelLayout& panelLayout() {
    static const PanelLayout layout = build();
    return layout;
}

} // namespace tannhauser
