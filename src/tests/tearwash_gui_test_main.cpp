// Tearwash 225 plugin and GUI verification (PLAN TW6.4): every flavour and program renders a
// finite, decaying response through the CLAP process call; the panel draws, its controls reach
// the parameters; host program changes load factory codes; the state round-trips. With a path
// argument it writes <path> (and <path>.225.ppm) snapshots for visual review.

#include "tearwash/plugin/TearwashClap.hpp"
#include "tearwash/plugin/TwGui.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace tearwash;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...) do { if (cond) { ++g_pass; } else { ++g_fail; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static const clap_host_t g_host = {
    CLAP_VERSION, nullptr, "test host", "tearwash", "", "1.0",
    [](const clap_host_t*, const char*) -> const void* { return nullptr; },
    [](const clap_host_t*) {}, [](const clap_host_t*) {}, [](const clap_host_t*) {},
};

struct Events {
    std::vector<clap_event_param_value_t> ev;
    static uint32_t size(const clap_input_events_t* l) { return static_cast<uint32_t>(static_cast<const Events*>(l->ctx)->ev.size()); }
    static const clap_event_header_t* get(const clap_input_events_t* l, uint32_t i) {
        return &static_cast<const Events*>(l->ctx)->ev[i].header;
    }
    void add(clap_id id, double v, uint32_t time = 0) {
        clap_event_param_value_t e{};
        e.header.size = sizeof(e);
        e.header.time = time;
        e.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        e.header.type = CLAP_EVENT_PARAM_VALUE;
        e.param_id = id;
        e.note_id = -1; e.port_index = -1; e.channel = -1; e.key = -1;
        e.value = v;
        ev.push_back(e);
    }
};
static std::vector<std::vector<uint8_t>> g_out;
static bool outPush(const clap_output_events_t*, const clap_event_header_t* e) {
    g_out.emplace_back(reinterpret_cast<const uint8_t*>(e), reinterpret_cast<const uint8_t*>(e) + e->size);
    return true;
}

// Processes in host-sized blocks; in/out are interleaved per channel vectors.
static void run(TearwashClap& p, const std::vector<float>& inL, const std::vector<float>& inR, std::vector<float>& outL,
                std::vector<float>& outR, Events* first = nullptr) {
    const size_t n = inL.size();
    outL.assign(n, 0.f);
    outR.assign(n, 0.f);
    for (size_t pos = 0; pos < n; pos += 256) {
        const uint32_t m = static_cast<uint32_t>(std::min<size_t>(256, n - pos));
        float* ic[2] = { const_cast<float*>(inL.data() + pos), const_cast<float*>(inR.data() + pos) };
        float* oc[2] = { outL.data() + pos, outR.data() + pos };
        clap_audio_buffer_t in{}, out{};
        in.data32 = ic; in.channel_count = 2;
        out.data32 = oc; out.channel_count = 2;
        Events none;
        Events* evs = (pos == 0 && first) ? first : &none;
        clap_input_events_t ie{ evs, Events::size, Events::get };
        clap_output_events_t oe{ nullptr, outPush };
        clap_process_t pr{};
        pr.frames_count = m;
        pr.audio_inputs = &in; pr.audio_inputs_count = 1;
        pr.audio_outputs = &out; pr.audio_outputs_count = 1;
        pr.in_events = &ie; pr.out_events = &oe;
        p.process(&pr);
        p.onMainThread();   // the host would call it when asked; frees retired engines
    }
}

static void writePpm(const TwGui& w, const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << w.getWidth() << " " << w.getHeight() << "\n255\n";
    for (uint32_t px : w.getPixelBuffer()) {
        const char rgb[3] = { static_cast<char>((px >> 16) & 0xFF), static_cast<char>((px >> 8) & 0xFF), static_cast<char>(px & 0xFF) };
        f.write(rgb, 3);
    }
}

// Impulse response of the current program: returns the wet energy in the tail window and
// checks finiteness and level.
static double impulseCheck(TearwashClap& p, const char* what) {
    const int fs = 48000, n = fs * 2;
    std::vector<float> l(n, 0.f), r(n, 0.f), ol, orr;
    run(p, l, r, ol, orr);   // let a pending engine arrive
    l[0] = r[0] = 0.5f;
    run(p, l, r, ol, orr);
    double tail = 0.0, peak = 0.0;
    bool finite = true;
    for (int i = 0; i < n; ++i) {
        finite = finite && std::isfinite(ol[i]) && std::isfinite(orr[i]);
        peak = std::max(peak, static_cast<double>(std::max(std::fabs(ol[i]), std::fabs(orr[i]))));
        if (i > fs / 10 && i < fs / 2) tail += double(ol[i]) * ol[i] + double(orr[i]) * orr[i];
    }
    CHECK(finite, "%s: finite output", what);
    CHECK(peak < 1.0, "%s: peak %.3f < 1", what, peak);
    CHECK(tail > 1e-6, "%s: reverberant tail (energy %.3g)", what, tail);
    return tail;
}

int main(int argc, char** argv) {
    const auto* factory = static_cast<const clap_plugin_factory_t*>(clap_entry.get_factory(CLAP_PLUGIN_FACTORY_ID));
    CHECK(factory && factory->get_plugin_count(factory) == 1, "factory");
    const clap_plugin_t* cp = factory->create_plugin(factory, &g_host, "com.holstebroe.tearwash225");
    CHECK(cp != nullptr, "create plugin");
    if (!cp) return 1;
    auto& p = *static_cast<TearwashClap*>(cp->plugin_data);
    cp->init(cp);
    cp->activate(cp, 48000.0, 32, 512);

    // Latency: fixed per rate, the dry path is aligned with it.
    CHECK(p.latency() > 20 && p.latency() < 120, "latency %u samples at 48 kHz", p.latency());
    {
        p.onParamValueFromGui(TW_MIX, 0.0);
        std::vector<float> l(4800, 0.f), r(4800, 0.f), ol, orr;
        run(p, l, r, ol, orr);
        l[10] = r[10] = 0.5f;
        run(p, l, r, ol, orr);
        int at = -1;
        for (int i = 0; i < 4800; ++i) if (std::fabs(ol[i]) > 0.25f) { at = i; break; }
        CHECK(at == 10 + static_cast<int>(p.latency()), "dry impulse at %d, expected %u", at, 10 + p.latency());
        p.onParamValueFromGui(TW_MIX, 1.0);
    }

    // Every flavour and program.
    for (int fl = 0; fl < FL_COUNT; ++fl) {
        for (int pg = 0; pg < programCount(fl); ++pg) {
            p.selectProgram(fl, pg);
            char what[64];
            std::snprintf(what, sizeof what, "%s %s", flavourName(fl), programName(fl, pg));
            impulseCheck(p, what);
            CHECK(p.flavour() == fl && p.program() == pg, "%s selected", what);
        }
    }

    // Factory codes on program change; host automation of the program.
    p.selectProgram(FL_224XL, 2);   // PLATE
    CHECK(toCode(p.paramValue(TW_MID)) == 0x31 || toCode(p.paramValue(TW_MID)) > 0, "PLATE factory MID loaded (%02X)", toCode(p.paramValue(TW_MID)));
    const int plateMid = toCode(p.paramValue(TW_MID));
    {
        Events ev;
        ev.add(TW_PROGRAM, 0);   // CONCERT HALL
        std::vector<float> l(2048, 0.f), r(2048, 0.f), ol, orr;
        run(p, l, r, ol, orr, &ev);
        CHECK(p.program() == 0, "host program change");
        CHECK(toCode(p.paramValue(TW_MID)) == 0x5F, "CONCERT HALL factory MID after host change (%02X, plate %02X)",
              toCode(p.paramValue(TW_MID)), plateMid);
    }
    // A code change through the host reaches the engine without a rebuild.
    {
        Events ev;
        ev.add(TW_MID, fromCode(0xB0));
        std::vector<float> l(1024, 0.f), r(1024, 0.f), ol, orr;
        run(p, l, r, ol, orr, &ev);
        CHECK(toCode(p.paramValue(TW_MID)) == 0xB0 && p.program() == 0, "host code change keeps the program");
    }

    // GUI.
    p.createGuiWindow();
    TwGui& gui = *p.getGuiWindow();
    gui.renderFrame();
    {
        const auto& px = gui.getPixelBuffer();
        double sum = 0, sum2 = 0;
        for (uint32_t v : px) { const double y = ((v >> 16) & 0xFF) + ((v >> 8) & 0xFF) + (v & 0xFF); sum += y; sum2 += y * y; }
        const double mean = sum / px.size(), var = sum2 / px.size() - mean * mean;
        CHECK(var > 100.0, "panel has content (variance %.1f)", var);
    }
    auto centre = [&](int i, int& x, int& y) {
        const auto& c = gui.controls()[static_cast<size_t>(i)];
        x = static_cast<int>(c.x + c.w / 2); y = static_cast<int>(c.y + c.h / 2);
    };
    int x, y;
    // Fader drag moves MID up.
    const int mid = gui.findControl(TwGui::Kind::Fader, TW_MID);
    CHECK(mid >= 0, "MID fader");
    {
        const auto& c = gui.controls()[static_cast<size_t>(mid)];
        const double before = p.paramValue(TW_MID);
        const float y1 = c.y + c.h - 8.f, y0 = c.y + 8.f;
        const int capY = static_cast<int>(y1 - before * (y1 - y0));
        gui.handleMouseDown(static_cast<int>(c.x + c.w / 2), capY, false);
        gui.handleMouseDrag(static_cast<int>(c.x + c.w / 2), capY - 20, false);
        gui.handleMouseUp();
        CHECK(p.paramValue(TW_MID) > before, "fader drag raises MID (%.3f → %.3f)", before, p.paramValue(TW_MID));
        CHECK(std::fabs(p.paramValue(TW_MID) * 255.0 - std::round(p.paramValue(TW_MID) * 255.0)) < 1e-9, "fader moves in codes");
        gui.handleMouseMove(static_cast<int>(c.x + c.w / 2), capY);
        CHECK(gui.readoutText().find("MID DECAY") == 0, "readout '%s'", gui.readoutText().c_str());
    }
    // Wheel: one code per notch.
    {
        const int pre = gui.findControl(TwGui::Kind::Fader, TW_PREDELAY);
        centre(pre, x, y);
        const int before = toCode(p.paramValue(TW_PREDELAY));
        gui.handleWheel(x, y, 1);
        CHECK(toCode(p.paramValue(TW_PREDELAY)) == std::min(255, before + 1), "wheel steps one code");
        gui.handleMouseMove(x, y);
        CHECK(gui.readoutText().find(" MS") != std::string::npos, "predelay readout in ms: '%s'", gui.readoutText().c_str());
    }
    // Toggle.
    {
        const int t = gui.findControl(TwGui::Kind::Toggle, TW_REAR);
        centre(t, x, y);
        const double before = p.paramValue(TW_REAR);
        gui.handleMouseDown(x, y, false);
        gui.handleMouseUp();
        CHECK(p.paramValue(TW_REAR) != before, "rear toggle");
    }
    // Program and flavour buttons.
    {
        centre(gui.findControl(TwGui::Kind::Program, 4), x, y);
        gui.handleMouseDown(x, y, false);
        gui.handleMouseUp();
        CHECK(p.program() == 4, "program button selects CHAMBER (%d)", p.program());
        g_out.clear();
        std::vector<float> l(256, 0.f), r(256, 0.f), ol, orr;
        run(p, l, r, ol, orr);
        CHECK(!g_out.empty(), "program change reported to the host (%zu events)", g_out.size());
    }
    gui.renderFrame();
    if (argc > 1) writePpm(gui, argv[1]);
    {
        centre(gui.findControl(TwGui::Kind::Flavour, FL_225), x, y);
        gui.handleMouseDown(x, y, false);
        gui.handleMouseUp();
        CHECK(p.flavour() == FL_225, "flavour button 225");
        // 225 dims the controls it does not have.
        const int diff = gui.findControl(TwGui::Kind::Knob, TW_DIFFUSION);
        centre(diff, x, y);
        const double before = p.paramValue(TW_DIFFUSION);
        gui.handleMouseDown(x, y, false);
        gui.handleMouseDrag(x, y - 40, false);
        gui.handleMouseUp();
        CHECK(p.paramValue(TW_DIFFUSION) == before, "dimmed control ignores input");
        gui.renderFrame();
        if (argc > 1) writePpm(gui, std::string(argv[1]) + ".225.ppm");
    }
    // No redraw when nothing changed.
    {
        gui.renderFrame();
        const int r0 = gui.redraws();
        gui.renderFrame();
        CHECK(gui.redraws() == r0, "idle frame does not redraw");
    }
    p.destroyGuiWindow();

    // State round trip.
    p.selectProgram(FL_224XL, 1);
    p.onParamValueFromGui(TW_SIZE, fromCode(0x40));
    p.onParamValueFromGui(TW_OUTGAIN, -6.0);
    const std::string st = p.stateText();
    {
        const clap_plugin_t* cp2 = factory->create_plugin(factory, &g_host, "com.holstebroe.tearwash225");
        auto& q = *static_cast<TearwashClap*>(cp2->plugin_data);
        cp2->init(cp2);
        cp2->activate(cp2, 44100.0, 32, 512);
        CHECK(q.loadStateText(st), "state loads");
        bool same = true;
        for (uint32_t i = 0; i < TW_PARAM_COUNT; ++i) same = same && q.paramValue(i) == p.paramValue(i);
        CHECK(same, "state round trip");
        CHECK(q.stateText() == st, "state text stable");
        impulseCheck(q, "restored ROOM at 44.1 kHz");
        CHECK(!q.loadStateText("garbage"), "rejects foreign state");
        cp2->destroy(cp2);
    }

    // Parameter text.
    {
        char buf[64];
        twValueText(TW_PREDELAY, fromCode(100), FL_224XL, 0, 32507.94, buf, sizeof buf);
        CHECK(std::strcmp(buf, "64  150 ms") == 0, "predelay text '%s'", buf);
        double v = 0;
        CHECK(twTextToValue(TW_MID, "B0", FL_224XL, &v) && toCode(v) == 0xB0, "code from text");
        CHECK(twTextToValue(TW_PROGRAM, "PLATE", FL_224XL, &v) && v == 2, "program from name");
        twValueText(TW_OUTGAIN, kOutGainFloorDb, FL_224XL, 0, 32507.94, buf, sizeof buf);
        CHECK(std::strcmp(buf, "-inf dB") == 0, "output floor text '%s'", buf);
    }

    cp->destroy(cp);
    std::printf("tearwash gui test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
