#ifndef TEARWASH_PROGRAMS_HPP
#define TEARWASH_PROGRAMS_HPP

// Algorithm networks on the virtual 224 core (docs/tearwash/03 §1). Each program is a native
// network: delay lines are regions of the shared delay memory (named offsets), allpass sections,
// one-pole filters and output taps are written out in the order the hardware evaluates them, so
// rounding, saturation and memory timing match the original [R]. Offsets and coefficients are
// members: the control layer (Control.hpp) sets them from the panel.

#include "Core.hpp"

namespace tearwash {

// Evaluation helpers shared by the networks. `a` is the accumulator, `r` the result register
// (it holds the previous sum while the next one accumulates: the hardware's one-step pipeline).
struct Pipe {
    CoreState& s;
    Mac& a;
    int16_t& r;
    inline int16_t tap(uint16_t o) const { return s.mem.read(o); }
    inline void store(uint16_t o, int16_t v) { s.mem.write(o, v); }
    // Latch the running sum into the result register and start a new sum.
    inline void next() { r = a.result(); a.zero(); }
    // Allpass section, four steps: input x from offset `xIn`, delayed d from `dIn`; stores the
    // previous section's output at `prevOut`, the section's own w = x + g·d at `wOut`; leaves
    // y = k·d − g·x accumulating (k is the separately quantised 1 − g², 02 §2).
    inline void allpass(uint16_t xIn, uint16_t dIn, uint16_t prevOut, uint16_t wOut, int g, int k) {
        const int16_t x = tap(xIn);
        next();
        a.add(x, 32);
        const int16_t d = tap(dIn);
        a.add(d, g);
        store(prevOut, r);
        next();
        a.add(d, k);
        store(wOut, r);
        a.add(x, -g);
    }
};

// Network interface: one call per core sample.
class Program {
public:
    virtual ~Program() = default;
    virtual void tick(CoreState& s) = 0;
    virtual int loopLength() const = 0;           // steps per sample (sets the core rate)
    virtual void reset() { a_ = Mac{}; r_ = 0; }
    // Seed the pipeline (accumulator, result register), e.g. from a hardware snapshot.
    void loadPipeline(int32_t acc, int16_t result) { a_.acc = acc; r_ = result; }
protected:
    Mac a_;
    int16_t r_ = 0;
};

// 224XL CONCERT HALL (also ROOM). Per channel: input bandwidth one-pole, four input diffusion
// allpasses, a modulated allpass in the tank, a one-pole into the decay section (crossover /
// treble), two decay allpasses, output taps. Left and right are cross-coupled through the tank.
// Defaults are the factory settings (all panel controls at their factory codes).
class ConcertHall final : public Program {
public:
    // Delay-memory offsets (SIZE stretches them, PREDELAY and DEPTH move some).
    struct Offsets {
        uint16_t carry = 128, lpL = 19678, lpR = 42606;
        uint16_t apL[4][2] = { { 2683, 4627 }, { 2515, 5239 }, { 5583, 9475 }, { 5755, 8211 } };   // {w, d}
        uint16_t apR[4][2] = { { 12679, 14711 }, { 12507, 15079 }, { 15579, 19659 }, { 15751, 18319 } };
        // Fractional tank taps: two adjacent reads, weights w and 32 - w. Mode Enhancement walks
        // them (Control.hpp); these are the resting positions.
        uint16_t modL[2] = { 254, 254 }, modR[2] = { 1, 1 };
        uint16_t tankL = 5927, tankR = 15923;           // modulated allpass w
        uint16_t tankOutL = 7395, tankOutR = 17631;
        uint16_t outB1 = 15103, outB2 = 9511, outD1 = 5515;
        uint16_t xoInL = 19675, xoInR = 11047, xoL = 258, xoR = 256;
        uint16_t fbL = 19680, fbR = 42608;
        uint16_t midL = 260, midR = 262, midTapL = 261, midTapR = 263;
        uint16_t dec1L[2] = { 277, 516 }, dec1R[2] = { 911, 1116 };   // {w, d}
        uint16_t dec2L[2] = { 518, 910 }, dec2R[2] = { 1117, 1446 };
        uint16_t decOutL = 1460, decOutR = 11047;
        uint16_t outA[3] = { 5279, 15271, 10023 }, outC[4] = { 15547, 11672, 5551, 19667 };
        uint16_t xferA = 17375, xferC = 7139, xferAOut = 0;
    };
    struct Coefs {
        int inA = 26, inB = 6;                          // HF BANDWIDTH pair
        int apG[4] = { 8, 13, 13, 8 }, apK[4] = { 30, 27, 27, 30 };   // DIFFUSION / DEFINITION
        int modLw = 32, modRw = 32;                     // fractional tap weights (w, 32 - w)
        int tankG = 8, tankK = 30;
        int outB1 = 30, outB2 = 15, outD1 = 30;
        int trebleA = 4, trebleB = 28;                  // TREBLE DECAY pair
        int fb = 11, xo = 5, fbTap = 16;                // LF DECAY − MID, CROSSOVER into the loop
        int midA = 10, midB = 22;                       // MID DECAY pair
        int dec1G = 12, dec1K = 27, dec2G = 10, dec2K = 29;
        int outA[4] = { 14, 30, -14, 4 }, outC[4] = { 14, 30, -14, 4 };
    };

    void tick(CoreState& s) override;
    int loopLength() const override { return 105; }
    Offsets o;
    Coefs c;
};

} // namespace tearwash

#endif
