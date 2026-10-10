#ifndef TEARWASH_CONTROL_HPP
#define TEARWASH_CONTROL_HPP

// Control layer: what the 224X/XL controller does while audio runs (docs/tearwash/02 §6,
// 03 §4). Timing is in core samples, so behaviour does not depend on the host rate.

#include <cstdint>

namespace tearwash {

// Mode Enhancement: the controller glides each modulated tank tap along its delay line.
// Position P is kept in 1/32 sample; the network reads the two neighbouring samples with weights
// (32 − frac, frac). Each update moves P by `step` units; every 8·N updates a new direction is
// drawn at random; a move that would leave the tap's window (offset bits under `windowMask`
// must stay as at home) is refused and reverses the direction. With `alternate`, odd taps move
// opposite to even ones. Measured on the original at CHORUS 80: ≈ 980 updates/s, step 1/32,
// N = 32, window mask 0x80 (CONCERT HALL) [R]. The original draws directions from a fixed byte
// table; this uses its own generator (same statistics, not the same sequence).
class ModWalker {
public:
    static constexpr int kMaxTaps = 4;

    void setup(double coreRate, int taps, double updatesPerSecond = 980.0, int step = 1, int segment = 32,
               uint16_t windowMask = 0x80, bool alternate = true) {
        taps_ = taps > kMaxTaps ? kMaxTaps : taps;
        interval_ = coreRate / updatesPerSecond;
        step_ = step;
        segment_ = 8 * (segment == 0 ? 256 : segment);
        mask_ = windowMask;
        alternate_ = alternate;
        reset();
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
        const int frac = pos_[i] & 31;
        w0 = 32 - frac;
    }

private:
    uint16_t offsetOf(int i, int p) const { return static_cast<uint16_t>(home_[i] + (p >> 5)); }
    uint32_t next() { rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5; return rng_; }
    int taps_ = 0, step_ = 1, segment_ = 256, count_ = 0, dir_ = 1;
    int pos_[kMaxTaps] = {};
    uint16_t home_[kMaxTaps] = {};
    uint16_t mask_ = 0x80;
    bool alternate_ = true;
    double interval_ = 33.0, phase_ = 0.0;
    uint32_t rng_ = 0x224C0DE5u;
};

} // namespace tearwash

#endif
