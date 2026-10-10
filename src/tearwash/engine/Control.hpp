#ifndef TEARWASH_CONTROL_HPP
#define TEARWASH_CONTROL_HPP

// Control layer: what the 224X/XL controller does while audio runs (docs/tearwash/02 §6,
// 03 §4). Timing is in core samples, so behaviour does not depend on the host rate.

#include <cmath>
#include <cstdint>

namespace tearwash {

// Panel state of a 224X/XL program as the controller stores it: one 8-bit register per slider
// (pages 1–12, six sliders each), SIZE, and the option bits (01 DYN DECAY, 40 MODE ENH,
// 80 DECAY OPT). The plugin and presets store these registers, as the original does.
struct XlRegs {
    uint8_t page[12][6] = {};
    uint8_t size = 0, size2 = 0;
    uint8_t options = 0xC0;
    uint8_t& at(int pg, int slider) { return page[pg - 1][slider - 1]; }   // 1-based, as on the LARC
    uint8_t at(int pg, int slider) const { return page[pg - 1][slider - 1]; }
};

namespace law {
// Most sliders use a 5-bit step: s = v >> 3, with 0 read as 1 (03 §2) [R].
inline int step5(uint8_t v) { const int s = v >> 3; return s ? s : 1; }
// Complementary one-pole pair (m, 32 − m): FD–FE give 32, FF switches both off [R].
inline void pair(uint8_t v, int& m, int& rest) {
    if (v == 0xFF) { m = rest = 0; return; }
    m = v >= 0xFD ? 32 : step5(v);
    rest = 32 - m;
}
// Allpass complement k for gain g (both /32): the separately rounded 1 − g² [R].
inline int allpassK(int g) { return 32 - ((g * g + 16) >> 5); }
// Millisecond law of PREDELAY: 1 ms steps to 50, then 2, 4 and 8 ms steps [R].
inline int predelayMs(uint8_t v) {
    if (v < 50) return v;
    if (v < 100) return 50 + 2 * (v - 50);
    if (v < 150) return 150 + 4 * (v - 100);
    return 350 + 8 * (v - 150);
}
// Delay from a coarse register and its fine register in units of u samples, rounded [R].
inline int fineDelay(uint8_t coarse, uint8_t fine, int u) { return (u * (256 * coarse + fine) + 128) >> 8; }
// Four-knot curve sampled at 3v in segments of 256 (knots at v = 0, 85.3, 170.7, 256), the step
// within a segment truncated toward zero [R]. FF is out of range on the original; read as FE.
inline int curve4(const int (&k)[4], uint8_t v) {
    if (v > 0xFE) v = 0xFE;
    const int x = 3 * v, i = x >> 8, f = x & 255;
    return k[i] + (k[i + 1] - k[i]) * f / 256;
}
// Decay-curve table row (values at s = 0, 8, 16, 24, 32): interpolate, truncate, halve [R].
inline int decayCurve(const int (&row)[5], int s) {
    const int i = s >> 3, f = s & 7;
    const int full = i >= 4 ? row[4] : row[i] + (row[i + 1] - row[i]) * f / 8;
    return full >> 1;
}
// Allpass gain from a scale/cap value byte: g = min(cap, ⌊scale·x/4⌋ / h) [R].
inline int scaledGain(int scale, int cap, int x, int h) {
    const int g = (scale * x / 4) / h;
    return g < cap ? g : cap;
}
// Decay Optimisation applied to a base gain: never below 1/32 once it was above 0 [R].
inline int reduced(int g, int steps) {
    const int r = g - steps;
    return g <= 0 ? g : (r < 1 ? 1 : r);
}
// SIZE (03 §3): the program's delay map stretches template offsets band by band [R].
// Below T2 nothing moves; T2..T0 stretches by r2, T0..T1 by r1 (both /16); the space above T1 is
// halved and each half moved as a block. r1 = 16·(lo + SIZE·(hi − lo)/256)/lo; r2 follows r1 by f/64,
// capped so the lower band cannot pass T0. Delay builders add their delay to S(base) + 1.
struct SizeMap {
    uint16_t t0 = 0, t1 = 0, t2 = 0, t3 = 0;
    int f = 0, lo = 16, hi = 16;
    int r1 = 16, r2 = 16;
    uint16_t e5 = 0, e7 = 0, mid = 0;
    bool active = false;                       // false: SIZE does not move anything
    void set(uint8_t size) {
        if (!active) return;
        r1 = (16 * (lo + (size * (hi - lo)) / 256)) / lo;
        if (f) {
            r2 = 16 + (f * (r1 - 16)) / 64;
            if (t3 != t2) { const int d = (16 * (t0 - t2)) / (t3 - t2); if (r2 > d) r2 = d; }
        } else {
            r2 = 16;
        }
        e5 = static_cast<uint16_t>(band(t1) + 1);
        mid = static_cast<uint16_t>(t1 + (0x10000 - t1) / 2);
        e7 = static_cast<uint16_t>(e5 + (0x10000 - e5) / 2);
    }
    uint16_t operator()(uint16_t x) const {
        if (!active) return x;
        if (x <= t1) return static_cast<uint16_t>(band(x));
        if (x < mid) return static_cast<uint16_t>(x - t1 + e5);
        return static_cast<uint16_t>(x - mid + e7);
    }
    // Largest delay (samples) a delay builder may add above the map: FFF6h − e7.
    int delayLimit() const { return active ? 0xFFF6 - e7 : 0xFFF6; }
private:
    int band(int x) const {
        if (x <= t2) return x;
        if (x < t0) return t2 + (r2 * (x - t2)) / 16;
        return t0 + (r1 * (x - t0)) / 16;
    }
};
// PREDELAY register clamp: the largest value whose delay fits under the map's limit [R].
inline uint8_t clampPredelay(uint8_t v, int limit) {
    while (v > 0 && 34 * predelayMs(v) > limit) --v;
    return v;
}
// CHORUS: modulation update divider and step (4 = 1/32 sample) from the 5-bit step s = v >> 3:
// below 80h the updates slow down (divider 17 − s), from 80h the steps grow (4·(s − 15)) [R].
inline void chorus(uint8_t v, int& divider, int& step4) {
    const int s = v >> 3;
    if (s < 16) { divider = 17 - s; step4 = 4; }
    else { divider = 1; step4 = 4 * (s - 15); }
}
} // namespace law

// Decay Optimisation (option 80). A level detector (≈ 55 Hz) turns the input peak into a level
// code (≈ 20 units per octave of peak, FF near full scale) and keeps a held peak code: a higher code
// replaces it and restarts a 3.7 s hold, after which it decays at a rate set by MID DECAY (so the
// diffusion returns about when the tail has died). A sudden fall (16h units below the last two
// readings; a gradual fade does not trigger it) starts the reduction: the MID-group allpass gains step down by 1/32 every ≈ 41 ms (at most 12
// steps, never below 1/32); when the held code reaches the floor, or the input comes back, they
// step up again. Traced on the original with noise bursts: ramp start 40 ms after the input
// stops; restore 5.5 / 6.1 / 6.75 s after the burst starts at 0.01 / 0.06 / 0.3 FS
// independent of burst length; restore at MID 30 / 5F / 70 / 90 / B0 / D0 after 5.6 / 6.1 /
// 6.8 / 8.2 / 10.2 / 15.0 s, none within 40 s at F0 [R]. Code scale and rate table are fitted to
// those points [D].
class DecayOptimiser {
public:
    void setup(double coreRate) {
        detectInterval_ = coreRate / 55.0;
        stepInterval_ = coreRate * 0.041;
        dt_ = 1.0 / 55.0;
        reset();
    }
    void reset() { peak_ = 0; held_ = prev1_ = prev2_ = 0.0; hold_ = 0.0; red_ = 0; ramping_ = false; dPhase_ = sPhase_ = 0.0; }
    // MID DECAY step s(MID) (1–31): decay rate of the held code, units per second.
    void setMid(int s) {
        static const double kS[] = { 1, 6, 11, 14, 18, 22, 26, 30, 31 };
        static const double kRate[] = { 140, 105, 80, 64, 44, 30, 17, 2, 1 };
        double v = s;
        if (v <= kS[0]) { rate_ = kRate[0]; return; }
        for (int i = 1; i < 9; ++i)
            if (v <= kS[i]) { rate_ = kRate[i - 1] + (kRate[i] - kRate[i - 1]) * (v - kS[i - 1]) / (kS[i] - kS[i - 1]); return; }
        rate_ = kRate[8];
    }
    // One core sample: input words L, R. Returns true when the reduction changed.
    bool tick(int16_t l, int16_t r, bool enabled, int maxSteps = 12) {
        const int a = l < 0 ? -l : l, b = r < 0 ? -r : r;
        if (a > peak_) peak_ = a;
        if (b > peak_) peak_ = b;
        if ((dPhase_ += 1.0) >= detectInterval_) {
            dPhase_ -= detectInterval_;
            double code = peak_ > 0 ? 255.0 + 19.6 * std::log2(peak_ / (0.95 * 32768.0)) : 0.0;
            code = code < 0.0 ? 0.0 : (code > 255.0 ? 255.0 : code);
            peak_ = 0;
            if (code > held_) { held_ = code; hold_ = 3.7; }
            else if (hold_ > 0.0) hold_ -= dt_;
            else held_ = held_ > rate_ * dt_ ? held_ - rate_ * dt_ : 0.0;
            // A sudden fall against the last two readings starts the ramp; it holds until the
            // held code reaches the floor or the input returns to the held level.
            const double recent = prev1_ > prev2_ ? prev1_ : prev2_;
            if (!ramping_ && code < recent - kDrop) ramping_ = true;
            if (ramping_ && (held_ <= kFloor || code >= held_ - kDrop)) ramping_ = false;
            prev2_ = prev1_;
            prev1_ = code;
        }
        if ((sPhase_ += 1.0) < stepInterval_) return false;
        sPhase_ -= stepInterval_;
        const int target = (enabled && ramping_) ? maxSteps : 0;
        if (red_ == target) return false;
        red_ += red_ < target ? 1 : -1;
        return true;
    }
    int reduction() const { return red_; }
private:
    static constexpr double kDrop = 22.0, kFloor = 16.0;   // 16h below the held code; floor 10h
    int peak_ = 0, red_ = 0;
    double held_ = 0.0, hold_ = 0.0, dt_ = 1.0 / 55.0, rate_ = 80.0, prev1_ = 0.0, prev2_ = 0.0;
    bool ramping_ = false;
    double detectInterval_ = 600.0, stepInterval_ = 1300.0, dPhase_ = 0.0, sPhase_ = 0.0;
};

// Mode Enhancement: the controller glides each modulated tank tap along its delay line.
// Position P is kept in 1/128 sample (the weight resolution is 1/32); the network reads the two
// neighbouring samples with weights (32 − frac, frac). Each update moves P by `step` units
// (4 = 1/32 sample, CHORUS 80); every 8·N updates a new direction is
// drawn at random; a move that would leave the tap's window (offset bits under `windowMask`
// must stay as at home) is refused and reverses the direction. With `alternate`, odd taps move
// opposite to even ones. Measured on the original at CHORUS 80: ≈ 980 updates/s, step 1/32,
// N = 32, window mask 0x80 (CONCERT HALL) [R]. The original draws directions from a fixed byte
// table; this uses its own generator (same statistics, not the same sequence).
class ModWalker {
public:
    static constexpr int kMaxTaps = 4;

    void setup(double coreRate, int taps, double updatesPerSecond = 980.0, int step = 4, int segment = 32,
               uint16_t windowMask = 0x80, bool alternate = true) {
        taps_ = taps > kMaxTaps ? kMaxTaps : taps;
        interval_ = coreRate / updatesPerSecond;
        step_ = step;
        segment_ = 8 * (segment == 0 ? 256 : segment);
        mask_ = windowMask;
        alternate_ = alternate;
        reset();
    }
    // Random sequence of the direction draws (the original uses a fixed table; any seed is as good).
    void setSeed(uint32_t seed) { rng_ = seed ? seed : 0x224C0DE5u; }
    // Change speed without disturbing the taps (CHORUS).
    void setSpeed(double coreRate, double updatesPerSecond, int step) {
        interval_ = coreRate / updatesPerSecond;
        step_ = step;
    }
    void reset() {
        phase_ = 0.0;
        count_ = 0;
        dir_ = 1;
        for (int i = 0; i < kMaxTaps; ++i) pos_[i] = 0;
    }
    // Home offset of tap i (its resting read position).
    void setHome(int i, uint16_t o) { home_[i] = o; }
    // Advance one core sample. Returns true when the tap positions changed.
    bool tick() {
        phase_ += 1.0;
        if (phase_ < interval_) return false;
        phase_ -= interval_;
        if (++count_ >= segment_) {
            count_ = 0;
            dir_ = (next() & 1) ? 1 : -1;
        }
        for (int i = 0; i < taps_; ++i) {
            const int d = (alternate_ && (i & 1)) ? -dir_ : dir_;
            const int p = pos_[i] + d * step_;
            const uint16_t o = offsetOf(i, p), o2 = static_cast<uint16_t>(o + 1);
            if (((o ^ home_[i]) & mask_) || ((o2 ^ home_[i]) & mask_)) {
                if (i == 0) dir_ = -dir_;
                continue;
            }
            pos_[i] = p;
        }
        return true;
    }
    // Tap i as two offsets and the weight of the first (out of 32).
    void tap(int i, uint16_t& o0, uint16_t& o1, int& w0) const {
        o0 = offsetOf(i, pos_[i]);
        o1 = static_cast<uint16_t>(o0 + 1);
        const int frac = (pos_[i] >> 2) & 31;
        w0 = 32 - frac;
    }

private:
    uint16_t offsetOf(int i, int p) const { return static_cast<uint16_t>(home_[i] + (p >> 7)); }
    uint32_t next() { rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5; return rng_; }
    int taps_ = 0, step_ = 4, segment_ = 256, count_ = 0, dir_ = 1;
    int pos_[kMaxTaps] = {};
    uint16_t home_[kMaxTaps] = {};
    uint16_t mask_ = 0x80;
    bool alternate_ = true;
    double interval_ = 33.0, phase_ = 0.0;
    uint32_t rng_ = 0x224C0DE5u;
};

} // namespace tearwash

#endif
