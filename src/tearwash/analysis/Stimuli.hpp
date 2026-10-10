#ifndef TEARWASH_STIMULI_HPP
#define TEARWASH_STIMULI_HPP

// Deterministic calibration stimuli (docs/tearwash/04_VALIDATION.md §2). The oracle driver and
// tearwash_calib regenerate the same signal from its name, so only the name is stored.

#include "Wav.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace tearwash {

struct Stimulus {
    Audio audio;          // 2 channels (L, R)
    size_t start = 0;     // first excited sample
    size_t stop = 0;      // first sample after the excitation
    bool valid = false;
};

constexpr double kStimPreroll = 0.05;   // s of silence before the excitation

constexpr double kSweepSeconds = 3.0;   // exponential sweep 20 Hz .. 20 kHz
constexpr double kSweepF1 = 20.0, kSweepF2 = 20000.0, kSweepAmp = 0.125;   // -18 dBFS: room for the 224X pre-emphasis

// Exponential (Farina) sweep sample i of n at `rate`.
inline double sweepSample(size_t i, size_t n, unsigned rate) {
    const double T = double(n) / rate, k = std::log(kSweepF2 / kSweepF1), t = double(i) / rate;
    const double fade = std::min({ 1.0, t / 0.01, (T - t) / 0.01 });
    return kSweepAmp * fade * std::sin(2.0 * 3.14159265358979 * kSweepF1 * T / k * (std::exp(t * k / T) - 1.0));
}

// Names: impulse, impulseL, impulseR, sweep, sweepL, sweepR, burst, sine1k. `seconds` is the total length.
inline Stimulus makeStimulus(const std::string& name, unsigned rate, double seconds) {
    Stimulus s;
    const size_t n = static_cast<size_t>(seconds * rate);
    s.audio.rate = rate;
    s.audio.ch.assign(2, std::vector<float>(n, 0.0f));
    s.start = static_cast<size_t>(kStimPreroll * rate);
    auto& L = s.audio.ch[0];
    auto& R = s.audio.ch[1];
    if (s.start >= n) return s;
    if (name == "impulse" || name == "impulseL" || name == "impulseR") {
        if (name != "impulseR") L[s.start] = 0.5f;
        if (name != "impulseL") R[s.start] = 0.5f;
        s.stop = s.start + 1;
    } else if (name == "sweep" || name == "sweepL" || name == "sweepR") {
        const size_t len = static_cast<size_t>(kSweepSeconds * rate);
        for (size_t i = 0; i < len && s.start + i < n; ++i) {
            const float v = static_cast<float>(sweepSample(i, len, rate));
            if (name != "sweepR") L[s.start + i] = v;
            if (name != "sweepL") R[s.start + i] = v;
        }
        s.stop = s.start + len;
    } else if (name == "burst") {
        // White Gaussian noise, 0.3 s at -18 dBFS RMS, identical in L and R.
        uint64_t x = 0x7EA2A5225ull;
        const size_t len = static_cast<size_t>(0.3 * rate);
        for (size_t i = 0; i < len && s.start + i < n; ++i) {
            double g = 0.0;
            for (int k = 0; k < 4; ++k) {
                x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
                g += static_cast<double>((x * 0x2545F4914F6CDD1Dull) >> 11) * (1.0 / 9007199254740992.0);
            }
            const float v = static_cast<float>((g - 2.0) * 1.7320508 * 0.1259);
            L[s.start + i] = R[s.start + i] = v;
        }
        s.stop = s.start + len;
    } else if (name == "sine1k") {
        // 1 kHz at -12 dBFS peak for 3 s, 5 ms raised-cosine edges.
        const size_t len = 3 * rate, edge = static_cast<size_t>(0.005 * rate);
        for (size_t i = 0; i < len && s.start + i < n; ++i) {
            double env = 1.0;
            if (i < edge) env = 0.5 - 0.5 * std::cos(3.14159265358979 * double(i) / edge);
            else if (i >= len - edge) env = 0.5 - 0.5 * std::cos(3.14159265358979 * double(len - i) / edge);
            L[s.start + i] = R[s.start + i] =
                static_cast<float>(0.251 * env * std::sin(2.0 * 3.14159265358979 * 1000.0 * double(i) / rate));
        }
        s.stop = s.start + len;
    } else {
        return s;
    }
    s.valid = true;
    return s;
}

} // namespace tearwash

#endif
