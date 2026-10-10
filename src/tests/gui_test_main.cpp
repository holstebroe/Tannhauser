// GUI and plugin verification (docs/spec/07_VALIDATION.md §2, T14): renders
// the panel offscreen, exercises the controls and the preset menu, checks
// the state round trip. With a path argument it writes <path> and
// <path>.menu.ppm snapshots for visual review.

#include "clap/TannhauserClap.hpp"
#include "gui/GuiWindow.hpp"
#include "gui/PanelRenderer.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace tannhauser;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...) do { if (cond) { ++g_pass; } else { ++g_fail; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static const clap_host_t g_host = {
    CLAP_VERSION, nullptr, "test host", "tannhauser", "", "1.0",
    [](const clap_host_t*, const char*) -> const void* { return nullptr; },
    [](const clap_host_t*) {}, [](const clap_host_t*) {}, [](const clap_host_t*) {},
};

// Minimal event lists for process().
static uint32_t emptySize(const clap_input_events_t*) { return 0; }
static const clap_event_header_t* emptyGet(const clap_input_events_t*, uint32_t) { return nullptr; }
static std::vector<std::vector<uint8_t>> g_out;
static bool outPush(const clap_output_events_t*, const clap_event_header_t* e) {
    g_out.emplace_back(reinterpret_cast<const uint8_t*>(e), reinterpret_cast<const uint8_t*>(e) + e->size);
    return true;
}

static float processBlock(TannhauserClap& p, int frames) {
    std::vector<float> l(frames), r(frames);
    float* chans[2] = { l.data(), r.data() };
    clap_audio_buffer_t out{};
    out.data32 = chans;
    out.channel_count = 2;
    clap_input_events_t in{ nullptr, emptySize, emptyGet };
    clap_output_events_t oe{ nullptr, outPush };
    clap_process_t pr{};
    pr.frames_count = static_cast<uint32_t>(frames);
    pr.audio_outputs = &out;
    pr.audio_outputs_count = 1;
    pr.in_events = &in;
    pr.out_events = &oe;
    p.process(&pr);
    float peak = 0.f;
    for (float v : l) peak = std::max(peak, std::fabs(v));
    return peak;
}

static void writePpm(const GuiWindow& w, const char* path) {
    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << w.getWidth() << " " << w.getHeight() << "\n255\n";
    for (uint32_t px : w.getPixelBuffer()) {
        const char rgb[3] = { static_cast<char>((px >> 16) & 0xFF), static_cast<char>((px >> 8) & 0xFF), static_cast<char>(px & 0xFF) };
        f.write(rgb, 3);
    }
}

int main(int argc, char** argv) {
    const auto* plugin = clap_entry.get_factory(CLAP_PLUGIN_FACTORY_ID);
    const auto* factory = static_cast<const clap_plugin_factory_t*>(plugin);
    CHECK(factory && factory->get_plugin_count(factory) == 1, "factory");
    const clap_plugin_t* cp = factory->create_plugin(factory, &g_host, "com.holstebroe.tannhauser");
    CHECK(cp != nullptr, "create plugin");
    if (!cp) return 1;
    auto& p = *static_cast<TannhauserClap*>(cp->plugin_data);
    cp->init(cp);
    cp->activate(cp, 48000.0, 32, 1024);

    p.createGuiWindow();
    GuiWindow& gui = *p.getGuiWindow();
    const auto t0 = std::chrono::steady_clock::now();
    gui.renderFrame();
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::printf("first frame %.1f ms\n", ms);
    const auto& px = gui.getPixelBuffer();
    size_t distinct = 0;
    for (size_t i = 1; i < px.size(); ++i) distinct += px[i] != px[i - 1];
    CHECK(distinct > 10000, "panel looks empty (%zu transitions)", distinct);

    // Every control hit-tests to itself at its centre.
    const auto& L = panelLayout();
    for (size_t i = 0; i < L.controls.size(); ++i) {
        const Ctl& c = L.controls[i];
        const int hit = gui.controlIndexAt(c.x + c.w / 2, c.y + c.h / 2);
        CHECK(hit == static_cast<int>(i), "control %zu (%s) hit-tests to %d", i, c.label, hit);
        CHECK(c.x >= 0 && c.y >= 0 && c.x + c.w <= PanelLayout::kWidth && c.y + c.h <= PanelLayout::kHeight,
              "control %zu (%s) outside the window", i, c.label);
    }

    // Printed names fit their column at the one label size (spec 06: shorten, don't shrink).
    for (const Ctl& c : L.controls) {
        CHECK(panel::nameWidth(c.label) <= panel::nameRoom(c), "name '%s' is %.1f px wide, room %.1f",
              c.label ? c.label : "", panel::nameWidth(c.label), panel::nameRoom(c));
    }
    // Every tooltip (name: value, description) fits the header readout.
    {
        int rx, ry, rw, rh;
        panel::readoutBounds(rx, ry, rw, rh);
        for (uint32_t id = 0; id < PARAM_COUNT; ++id) {
            char val[64];
            paramValueText(id, paramInfo(id).max, val, sizeof val, true);
            std::string first = std::string(paramInfo(id).name) + ":  " + val, second = paramDescription(id);
            for (auto* t : { &first, &second }) for (auto& ch : *t) if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 32);
            CHECK(panel::readoutWidth(first, true) <= rw - 8 && panel::readoutWidth(second, false) <= rw - 8,
                  "tooltip of %s too wide (%.0f / %.0f px, room %d)", paramInfo(id).name,
                  panel::readoutWidth(first, true), panel::readoutWidth(second, false), rw - 8);
        }
    }

    // Slider drag: line I LPF up.
    const uint32_t lpf = lineParam(0, LP_LPF);
    for (const Ctl& c : L.controls) {
        if (c.param != static_cast<int>(lpf)) continue;
        const double before = p.paramValue(lpf);
        g_out.clear();
        gui.handleMouseDown(c.x + c.w / 2, c.y + c.h / 2, false);
        gui.handleMouseDrag(c.x + c.w / 2, c.y + c.h / 2 - 20, false);
        gui.handleMouseUp();
        processBlock(p, 64);
        CHECK(p.paramValue(lpf) > before + 0.1, "LPF drag %.3f -> %.3f", before, p.paramValue(lpf));
        int gestures = 0;
        for (const auto& e : g_out) {
            const auto* h = reinterpret_cast<const clap_event_header_t*>(e.data());
            if (h->type == CLAP_EVENT_PARAM_GESTURE_BEGIN || h->type == CLAP_EVENT_PARAM_GESTURE_END) ++gestures;
        }
        CHECK(gestures == 2, "slider drag gestures %d", gestures);
    }
    // Rocker toggle: line I square.
    for (const Ctl& c : L.controls) {
        if (c.param != static_cast<int>(lineParam(0, LP_SQUARE))) continue;
        const double before = p.paramValue(static_cast<clap_id>(c.param));
        gui.handleMouseDown(c.x + c.w / 2, c.y + c.h / 2, false);
        gui.handleMouseUp();
        CHECK(p.paramValue(static_cast<clap_id>(c.param)) != before, "rocker did not toggle");
    }

    // Ribbon Hold key toggles its parameter.
    for (const Ctl& c : L.controls) {
        if (c.type != CtlType::Toggle) continue;
        const double before = p.paramValue(static_cast<clap_id>(c.param));
        gui.handleMouseDown(c.x + c.w / 2, c.y + c.h / 2, false);
        gui.handleMouseUp();
        CHECK(p.paramValue(static_cast<clap_id>(c.param)) != before, "toggle %s did not toggle", c.label);
        gui.handleMouseDown(c.x + c.w / 2, c.y + c.h / 2, false);
        gui.handleMouseUp();
        CHECK(p.paramValue(static_cast<clap_id>(c.param)) == before, "toggle %s did not toggle back", c.label);
    }

    // Preset menu: open, pick the second category's first preset.
    for (const Ctl& c : L.controls) {
        if (c.type != CtlType::PresetLcd) continue;
        gui.handleMouseDown(c.x + c.w / 2, c.y + c.h / 2, false);
        gui.handleMouseUp();
        CHECK(gui.isMenuOpen(), "menu did not open");
        int cat = -1;
        for (int i = 0; i < gui.menuItemCount(0); ++i) {
            if (gui.menuItemLabel(0, i).rfind("FC ", 0) == 0) cat = i;
        }
        CHECK(cat >= 0, "menu has no FC category");
        int x, y;
        if (cat >= 0 && gui.menuItemCenter(0, cat, x, y)) {
            gui.handleMouseMove(x, y);
            CHECK(gui.menuItemCount(1) == 11, "FC submenu has %d items", gui.menuItemCount(1));
            if (gui.menuItemCenter(1, 2, x, y)) {
                gui.handleMouseMove(x, y);
                gui.renderFrame();
                if (argc > 1) writePpm(gui, (std::string(argv[1]) + ".menu.ppm").c_str());
                gui.handleMouseDown(x, y, false);
                gui.handleMouseUp();
            }
        }
        CHECK(!gui.isMenuOpen(), "menu did not close after a pick");
        CHECK(p.currentPresetName() == "FC Brass 1+2", "picked preset is '%s'", p.currentPresetName().c_str());
        CHECK(!p.isPresetModified(), "fresh preset marked modified");
    }

    // Tone button: Organ 1 into line I.
    for (const Ctl& c : L.controls) {
        if (c.type != CtlType::ToneButton || c.aux != 7) continue;
        gui.handleMouseDown(c.x + c.w / 2, c.y + c.h / 2, false);
        gui.handleMouseUp();
        const int idx = p.library().indexOf("FT Organ 1");
        CHECK(idx >= 0 && std::fabs(p.paramValue(lineParam(0, LP_LPF)) - p.library().presets()[idx].values[lineParam(0, LP_LPF)]) < 1e-9,
              "tone button did not load Organ 1 into line I");
        CHECK(p.isPresetModified(), "tone load should mark the preset modified");
    }

    // Memory: store line I in M1 (shift-click), change, recall.
    for (const Ctl& c : L.controls) {
        if (c.type != CtlType::ToneButton || c.aux != 11) continue;
        const double v = p.paramValue(lineParam(0, LP_LPF));
        gui.handleMouseDown(c.x + c.w / 2, c.y + c.h / 2, true);
        gui.handleMouseUp();
        p.onParamValueFromGui(lineParam(0, LP_LPF), 0.05);
        gui.handleMouseDown(c.x + c.w / 2, c.y + c.h / 2, false);
        gui.handleMouseUp();
        CHECK(std::fabs(p.paramValue(lineParam(0, LP_LPF)) - v) < 1e-9, "memory recall %.3f want %.3f", p.paramValue(lineParam(0, LP_LPF)), v);
    }

    // Keyboard click plays a note.
    for (const Ctl& c : L.controls) {
        if (c.type != CtlType::Keyboard) continue;
        gui.handleMouseDown(c.x + 5, c.y + c.h - 10, false);
        float peak = 0.f;
        for (int i = 0; i < 40; ++i) peak = std::max(peak, processBlock(p, 256));
        CHECK(peak > 0.01f, "keyboard click is silent (peak %.4f)", peak);
        CHECK(p.isKeyDown(36), "C2 not shown as down");
        gui.handleMouseUp();
        processBlock(p, 64);
        CHECK(!p.isKeyDown(36), "C2 still down");
    }

    // T14: state round trip.
    p.onParamValueFromGui(lineParam(1, LP_HPF), 0.37);
    p.onParamValueFromGui(P_SUB_FUNC, 4);
    const std::string text = p.stateText();
    const clap_plugin_t* cp2 = factory->create_plugin(factory, &g_host, "com.holstebroe.tannhauser");
    auto& p2 = *static_cast<TannhauserClap*>(cp2->plugin_data);
    CHECK(p2.loadStateText(text), "state load failed");
    int diffs = 0;
    for (uint32_t i = 0; i < PARAM_COUNT; ++i) diffs += std::fabs(p.paramValue(i) - p2.paramValue(i)) > 1e-5;
    CHECK(diffs == 0, "T14 state round trip: %d parameters differ", diffs);
    CHECK(p2.hasMemory(0), "T14 memory slot lost");
    CHECK(p2.currentPresetName() == p.currentPresetName(), "T14 preset name");
    cp2->destroy(cp2);

    // Render a few frames with hover to exercise the incremental path.
    gui.handleMouseMove(140, 100);
    gui.renderFrame();
    // The snapshot hovers line I IL, so it shows a two-line tooltip.
    {
        const Ctl* il = nullptr;
        for (const Ctl& c : L.controls) if (c.param == static_cast<int>(lineParam(0, LP_IL))) il = &c;
        if (il) gui.handleMouseMove(il->x + il->w / 2, il->y + il->h / 2);
        gui.renderFrame();
        CHECK(gui.readoutText().find('\n') != std::string::npos, "tooltip has a description line: '%s'", gui.readoutText().c_str());
    }
    if (argc > 1) writePpm(gui, argv[1]);

    p.destroyGuiWindow();
    cp->destroy(cp);
    std::printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
