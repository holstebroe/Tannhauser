#ifndef TANNHAUSER_LOUDNESS_HPP
#define TANNHAUSER_LOUDNESS_HPP

// Preset loudness measurement (spec 05 §4, test T16): ITU-R BS.1770
// K-weighted momentary loudness (400 ms windows, 100 ms hop) at 48 kHz.
// A preset's loudness is the mean (in LU) of the maximum momentary loudness
// of two phrases: a held six-note chord and a twelve-note line, both at
// velocity 0.75 and no aftertouch.

#include "core/SynthEngine.hpp"
#include "presets/Presets.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace tannhauser {

constexpr double kLoudnessTarget = -18.0;   // LUFS (momentary max), see tools/fit_preset_gains

struct KWeight {
    // BS.1770 stage 1 (high shelf) and stage 2 (RLB high-pass), 48 kHz coefficients.
    double x1[2]{}, x2[2]{}, y1[2]{}, y2[2]{};
    double tick(double x) {
        static const double b[2][3] = { { 1.53512485958697, -2.69169618940638, 1.19839281085285 }, { 1.0, -2.0, 1.0 } };
        static const double a[2][3] = { { 1.0, -1.69065929318241, 0.73248077421585 }, { 1.0, -1.99004745483398, 0.99007225036621 } };
        for (int s = 0; s < 2; ++s) {
            const double y = b[s][0] * x + b[s][1] * x1[s] + b[s][2] * x2[s] - a[s][1] * y1[s] - a[s][2] * y2[s];
            x2[s] = x1[s]; x1[s] = x; y2[s] = y1[s]; y1[s] = y;
            x = y;
        }
        return x;
    }
};

// Max momentary loudness (LUFS) of a stereo signal at 48 kHz.
inline double maxMomentaryLufs(const std::vector<float>& l, const std::vector<float>& r) {
    const int sr = 48000, win = sr * 4 / 10, hop = sr / 10;
    KWeight kl, kr;
    std::vector<double> e(l.size());
    for (size_t i = 0; i < l.size(); ++i) {
        const double a = kl.tick(l[i]), b = kr.tick(r[i]);
        e[i] = a * a + b * b;
    }
    double best = 1e-12;
    for (size_t start = 0; start + win <= e.size(); start += hop) {
        double s = 0.0;
        for (int i = 0; i < win; ++i) s += e[start + i];
        best = std::max(best, s / win);
    }
    return -0.691 + 10.0 * std::log10(best);
}

// Loudness of a preset (LUFS). `gainDb` replaces the preset's own Patch Gain when finite.
inline double presetLoudness(const ParamValues& values, double gainDb = NAN) {
    const int sr = 48000;
    double sum = 0.0;
    for (int phrase = 0; phrase < 2; ++phrase) {
        SynthEngine e;
        e.setSampleRate(sr);
        for (uint32_t i = 0; i < PARAM_COUNT; ++i) e.setParam(i, values[i]);
        if (std::isfinite(gainDb)) e.setParam(P_GAIN, gainDb);
        const double len = phrase == 0 ? 3.0 : 3.5;
        std::vector<float> l(static_cast<size_t>(len * sr)), r(l.size());
        struct Ev { double t; int key; bool on; };
        std::vector<Ev> ev;
        if (phrase == 0) {
            for (int k : { 48, 55, 60, 64, 67, 71 }) { ev.push_back({ 0.0, k, true }); ev.push_back({ 2.0, k, false }); }
        } else {
            const int keys[12] = { 60, 63, 67, 70, 72, 70, 67, 63, 65, 68, 72, 75 };
            for (int i = 0; i < 12; ++i) { ev.push_back({ 0.25 * i, keys[i], true }); ev.push_back({ 0.25 * i + 0.22, keys[i], false }); }
        }
        std::sort(ev.begin(), ev.end(), [](const Ev& a, const Ev& b) { return a.t < b.t; });
        size_t pos = 0, ei = 0;
        while (pos < l.size()) {
            while (ei < ev.size() && static_cast<size_t>(ev[ei].t * sr) <= pos) {
                if (ev[ei].on) e.noteOn(ev[ei].key, 0.75); else e.noteOff(ev[ei].key);
                ++ei;
            }
            size_t next = l.size();
            if (ei < ev.size()) next = std::min(next, static_cast<size_t>(ev[ei].t * sr));
            const int n = static_cast<int>(std::min<size_t>(std::max<size_t>(next - pos, 1), 512));
            e.process(&l[pos], &r[pos], n);
            pos += static_cast<size_t>(n);
        }
        sum += maxMomentaryLufs(l, r);
    }
    return sum / 2.0;
}

} // namespace tannhauser

#endif
