#ifndef TEARWASH_CORE_HPP
#define TEARWASH_CORE_HPP

// The virtual 224 hardware (docs/tearwash/02 §2–4): one shared circular delay memory with a
// position counter, the multiply-accumulate with the hardware's truncation and 19-bit
// saturation, and the floating-point converter's quantisation. Algorithms (Programs*.cpp) are
// written on these primitives. Real-time safe: no allocation after construction.

#include <cstdint>
#include <cstring>
#include <vector>

namespace tearwash {

// Delay memory: 64 K words of 16 bits. Every delay line of a program is a region of this ring;
// a value written at offset o is read back at offset o + d exactly d samples later.
class DelayMemory {
public:
    DelayMemory() : mem_(65536, 0) {}
    void clear() { std::memset(mem_.data(), 0, mem_.size() * sizeof(int16_t)); pos_ = 0; }
    inline int16_t read(uint16_t offset) const { return mem_[uint16_t(pos_ - offset)]; }
    inline void write(uint16_t offset, int16_t v) { mem_[uint16_t(pos_ - offset)] = v; }
    inline void advance() { ++pos_; }
    uint16_t position() const { return pos_; }
    void setPosition(uint16_t p) { pos_ = p; }
    int16_t* data() { return mem_.data(); }
private:
    std::vector<int16_t> mem_;
    uint16_t pos_ = 0;
};

// Multiply-accumulate unit. The accumulator holds 1/8 of a data LSB; products are formed per
// pair of coefficient bits with the hardware's truncation (each partial product rounds down and
// carries a +1 from the inverting adder stage), sums saturate to 19 bits, and a result is the
// accumulator >> 3. "Zero" restarts the sum at -1 (the hardware's cleared state). The bias this
// leaves is real: it is why every 224 output carries a small DC offset (02 §3).
struct Mac {
    int32_t acc = -1;

    inline void zero() { acc = -1; }
    // acc += c/32 · x, c = signed 6-bit coefficient (-63..63).
    inline void add(int32_t x, int c) {
        const int m = c < 0 ? -c : c;
        const int32_t p = (m >> 2) * (x + 1) + ((m >> 1) & 1) * ((x >> 1) + 1) + (m & 1) * ((x >> 2) + 1);
        const int32_t s = c < 0 ? acc - p : acc + p;
        acc = s < -262144 ? -262144 : (s > 262143 ? 262143 : s);
    }
    inline int16_t result() const { return static_cast<int16_t>(acc >> 3); }
};

// Floating-point converter quantisation (02 §4), both directions: a 12-bit mantissa with a
// 2-bit gain range, i.e. the word keeps its top 12 significant bits with at most 3 shifts.
inline int16_t fpcQuantize(int16_t y) {
    const int mag = y ^ (y >> 15);
    const int k = mag >= 0x4000 ? 4 : mag >= 0x2000 ? 3 : mag >= 0x1000 ? 2 : 1;
    return static_cast<int16_t>((y >> k) * (1 << k));
}

// State shared by every program network: memory, the result register (it carries over from one
// sample to the next), the converter inputs and the four DAC outputs A–D.
struct CoreState {
    DelayMemory mem;
    int16_t result = 0;
    int16_t inL = 0, inR = 0;      // FPC-quantised inputs of the current sample
    int16_t dac[4] = { 0, 0, 0, 0 };
    void clear() { mem.clear(); result = 0; inL = inR = 0; dac[0] = dac[1] = dac[2] = dac[3] = 0; }
};

} // namespace tearwash

#endif
