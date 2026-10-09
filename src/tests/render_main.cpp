// Offline renderer: plays a fixed phrase through a preset into a WAV file
// (spec 07 §3). Usage:
//   tannhauser_render "<preset name>" out.wav [phrase] [samplerate]
//   tannhauser_render --list
// Phrases: chord (default), line, swell, bass, all (chord + line + swell).

#include "core/SynthEngine.hpp"
#include "presets/Presets.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace tannhauser;

struct Ev { double t; int type; int key; double v; };   // type 0 on, 1 off, 2 pressure

static void phraseEvents(const std::string& phrase, double t0, std::vector<Ev>& ev, double& end) {
    if (phrase == "chord") {
        const int keys[] = { 48, 55, 60, 64, 67, 71 };
        for (int k : keys) ev.push_back({ t0, 0, k, 0.75 });
        for (int k : keys) ev.push_back({ t0 + 2.5, 1, k, 0 });
        end = t0 + 5.0;
    } else if (phrase == "line") {
        const int keys[] = { 60, 63, 67, 70, 72, 70, 67, 63, 65, 68, 72, 75 };
        double t = t0;
        for (int i = 0; i < 12; ++i) {
            ev.push_back({ t, 0, keys[i], 0.5 + 0.04 * i });
            ev.push_back({ t + 0.22, 1, keys[i], 0 });
            t += 0.25;
        }
        end = t + 2.0;
    } else if (phrase == "bass") {
        const int keys[] = { 36, 36, 48, 36, 39, 41, 43, 46 };
        double t = t0;
        for (int i = 0; i < 8; ++i) {
            ev.push_back({ t, 0, keys[i], 0.9 });
            ev.push_back({ t + 0.18, 1, keys[i], 0 });
            t += 0.25;
        }
        end = t + 1.5;
    } else {   // swell: chord with an aftertouch ramp on the top note
        const int keys[] = { 57, 60, 64, 69 };
        for (int k : keys) ev.push_back({ t0, 0, k, 0.5 });
        for (int i = 0; i <= 40; ++i) ev.push_back({ t0 + 0.5 + i * 0.05, 2, 69, i / 40.0 });
        for (int i = 0; i <= 40; ++i) ev.push_back({ t0 + 3.0 + i * 0.05, 2, 69, 1.0 - i / 40.0 });
        for (int k : keys) ev.push_back({ t0 + 5.5, 1, k, 0 });
        end = t0 + 8.0;
    }
}

static void writeWav(const std::string& path, const std::vector<float>& l, const std::vector<float>& r, int sr) {
    std::ofstream f(path, std::ios::binary);
    const uint32_t n = static_cast<uint32_t>(l.size());
    const uint32_t dataBytes = n * 2 * 2;
    auto u32 = [&](uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
    auto u16 = [&](uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };
    f.write("RIFF", 4); u32(36 + dataBytes); f.write("WAVE", 4);
    f.write("fmt ", 4); u32(16); u16(1); u16(2); u32(static_cast<uint32_t>(sr)); u32(static_cast<uint32_t>(sr) * 4); u16(4); u16(16);
    f.write("data", 4); u32(dataBytes);
    for (uint32_t i = 0; i < n; ++i) {
        for (float s : { l[i], r[i] }) {
            const float c = s > 1.f ? 1.f : (s < -1.f ? -1.f : s);
            u16(static_cast<uint16_t>(static_cast<int16_t>(c * 32767.f)));
        }
    }
}

int main(int argc, char** argv) {
    PresetLibrary lib;
    if (argc >= 2 && std::strcmp(argv[1], "--list") == 0) {
        for (const auto& p : lib.presets()) std::printf("%s\n", p.name.c_str());
        return 0;
    }
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s \"<preset>\" out.wav [chord|line|swell|bass|all] [samplerate]\n", argv[0]);
        return 1;
    }
    const int idx = lib.indexOf(argv[1]);
    if (idx < 0) { std::fprintf(stderr, "unknown preset '%s' (try --list)\n", argv[1]); return 1; }
    const std::string phrase = argc >= 4 ? argv[3] : "chord";
    const int sr = argc >= 5 ? std::atoi(argv[4]) : 48000;

    SynthEngine eng;
    eng.setSampleRate(sr);
    const ParamValues& v = lib.presets()[static_cast<size_t>(idx)].values;
    for (uint32_t i = 0; i < PARAM_COUNT; ++i) eng.setParam(i, v[i]);

    std::vector<Ev> ev;
    double end = 0.0;
    if (phrase == "all") {
        double t = 0.0;
        for (const char* ph : { "chord", "line", "swell" }) { phraseEvents(ph, t, ev, end); t = end; }
    } else {
        phraseEvents(phrase, 0.0, ev, end);
    }
    std::sort(ev.begin(), ev.end(), [](const Ev& a, const Ev& b) { return a.t < b.t; });
    const size_t total = static_cast<size_t>(end * sr);
    std::vector<float> L(total), R(total);
    size_t pos = 0, ei = 0;
    while (pos < total) {
        size_t next = total;
        while (ei < ev.size() && static_cast<size_t>(ev[ei].t * sr) <= pos) {
            const Ev& e = ev[ei++];
            if (e.type == 0) eng.noteOn(e.key, e.v);
            else if (e.type == 1) eng.noteOff(e.key);
            else eng.notePressure(e.key, e.v);
        }
        if (ei < ev.size()) next = std::min(total, static_cast<size_t>(ev[ei].t * sr));
        if (next <= pos) next = pos + 1;
        const int n = static_cast<int>(std::min<size_t>(next - pos, 256));
        eng.process(&L[pos], &R[pos], n);
        pos += static_cast<size_t>(n);
    }
    float peak = 0.f;
    double rms = 0.0;
    for (size_t i = 0; i < total; ++i) { peak = std::max(peak, std::fabs(L[i])); rms += L[i] * L[i]; }
    std::printf("%s: %.2f s, peak %.3f, rms %.4f\n", argv[1], end, peak, std::sqrt(rms / std::max<size_t>(1, total)));
    writeWav(argv[2], L, R, sr);
    return 0;
}
