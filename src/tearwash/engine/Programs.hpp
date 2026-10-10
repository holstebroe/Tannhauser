#ifndef TEARWASH_PROGRAMS_HPP
#define TEARWASH_PROGRAMS_HPP

// Algorithm networks on the virtual 224 core (docs/tearwash/03 §1). Each program is a native
// network: delay lines are regions of the shared delay memory (named offsets), allpass sections,
// one-pole filters and output taps are written out in the order the hardware evaluates them, so
// rounding, saturation and memory timing match the original [R]. Offsets and coefficients are
// members: the control layer (Control.hpp) sets them from the panel.

#include "Control.hpp"
#include "Core.hpp"

#include <memory>

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
    // Modulated taps (Mode Enhancement): count, resting offset, and the live position as two
    // adjacent offsets with the first one's weight (out of 32).
    virtual int modTaps() const { return 0; }
    virtual uint16_t modHome(int) const { return 0; }
    virtual void setModTap(int, uint16_t, uint16_t, int) {}
    // Walker constants of the program: segment length N and window mask (03 §4).
    virtual int modSegment() const { return 32; }
    virtual uint16_t modWindow() const { return 0x80; }
    // Apply the panel registers (sliders, SIZE, options) to offsets and coefficients.
    virtual void applyControls(const XlRegs&) {}
    // The predelayed input: its two read offsets and its gain, which the engine ramps to zero
    // while the offsets move (as the original's controller does). Null when the program has none.
    virtual void predelayRamp(uint16_t*& left, uint16_t*& right, int*& gain) { left = right = nullptr; gain = nullptr; }
    // Decay Optimisation: reduce the program's level-dependent allpass gains by `steps` (/32).
    virtual void setDecayReduction(int) {}
    virtual int decayReductionMax() const { return 12; }
    // Factory registers of the program.
    virtual XlRegs factory() const { return XlRegs{}; }
    // Seed the pipeline (accumulator, result register), e.g. from a hardware snapshot.
    void loadPipeline(int32_t acc, int16_t result) { a_.acc = acc; r_ = result; }
protected:
    Mac a_;
    int16_t r_ = 0;
};

// 224XL CONCERT HALL (also ROOM). Per channel: input bandwidth one-pole, four input diffusion
// allpasses, a modulated allpass in the tank, the decay section (crossover one-pole with the
// LF/MID loop gains, treble one-pole fed by the predelayed input, two diffusion allpasses), and
// output taps (DEPTH, pre-echoes). Left and right are cross-coupled through the tank. The
// defaults are the factory settings with Mode Enhancement at rest [R].
class ConcertHall final : public Program {
public:
    // Delay-memory offsets at the factory SIZE (PREDELAY, the pre-echo DELAYs and the two
    // decay taps move with their sliders).
    struct Offsets {
        uint16_t carry = 128, lpL = 19678, lpR = 42606;
        uint16_t apL[4][2] = { { 2683, 4627 }, { 2515, 5239 }, { 5583, 9475 }, { 5755, 8211 } };   // {w, d}
        uint16_t apR[4][2] = { { 12679, 14711 }, { 12507, 15079 }, { 15579, 19659 }, { 15751, 18319 } };
        uint16_t modL[2] = { 254, 254 }, modR[2] = { 1, 1 };   // fractional tank taps (two reads)
        uint16_t tankL = 5927, tankR = 15923;           // modulated allpass w
        uint16_t tankOutL = 7395, tankOutR = 17631;
        uint16_t outB1 = 15103, outB2 = 9511, outD1 = 5515;
        uint16_t xoInL = 19675, xoInR = 11047, xoL = 258, xoR = 256;   // crossover one-pole
        uint16_t preL = 19680, preR = 42608;            // predelayed input into the decay section
        uint16_t trebleL = 260, trebleR = 262;          // treble one-pole state
        uint16_t dif1L[2] = { 277, 516 }, dif1R[2] = { 911, 1116 };   // {w, d}
        uint16_t dif2L[2] = { 518, 910 }, dif2R[2] = { 1117, 1446 };
        uint16_t decOutL = 1460, decOutR = 11047;
        uint16_t outA[3] = { 5279, 15271, 10023 }, outC[4] = { 15547, 11672, 5551, 19667 };
        uint16_t echo[4] = { 19850, 42940, 43203, 20530 };   // pre-echo taps 1–4 (1, 3 → A; 2, 4 → C)
        uint16_t xferA = 17375, xferC = 7139, xferAOut = 0;
    };
    struct Coefs {
        int hfIn = 26, hfFb = 6;                        // HF BANDWIDTH pair (input, feedback)
        int apG[4] = { 8, 13, 13, 8 }, apK[4] = { 30, 27, 27, 30 };   // 0, 3: DEFINITION group; 1, 2: MID group
        int modLw = 32, modRw = 32;                     // fractional tap weights (w, 32 - w)
        int tankG = 8, tankK = 30;                      // DEFINITION group
        int outB1 = 30, outB2 = 15, outD1 = 30;
        int xoIn = 4, xoFb = 28;                        // CROSSOVER pair
        int mid = 11, lfDiff = 5;                       // MID loop gain, LF − MID through the crossover
        int pre = 16;                                   // predelayed input gain
        int trebleFb = 10, trebleIn = 22;               // TREBLE DECAY pair
        int dif1G = 12, dif1K = 27, dif2G = 10, dif2K = 29;   // DIFFUSION
        int outA[4] = { 14, 30, -14, 4 }, outC[4] = { 14, 30, -14, 4 };   // DEPTH
        int echo[4] = { 0, 0, 0, 0 };                   // pre-echo LEVELs
    };

    void tick(CoreState& s) override;
    int loopLength() const override { return 105; }
    int modTaps() const override { return 2; }
    uint16_t modHome(int i) const override { return i ? 1 : 254; }
    void setModTap(int i, uint16_t o0, uint16_t o1, int w) override {
        uint16_t* t = i ? o.modR : o.modL;
        t[0] = o0; t[1] = o1;
        (i ? c.modRw : c.modLw) = w;
    }
    void applyControls(const XlRegs& r) override;
    XlRegs factory() const override;
    static Offsets templateOffsets();          // offsets at the smallest SIZE (the map's input)
    void predelayRamp(uint16_t*& l, uint16_t*& r, int*& g) override { l = &o.preL; r = &o.preR; g = &c.pre; }
    void setDecayReduction(int steps) override {
        reduction_ = steps;
        const int g = law::reduced(gMid_, steps);
        c.apG[1] = c.apG[2] = g;
        c.apK[1] = c.apK[2] = law::allpassK(g);
    }
    Offsets o;
    Coefs c;
private:
    int gMid_ = 13, reduction_ = 0;
};

// 224XL PLATE (also SMALL PLATE). Per channel: input gain, an input allpass, a one-pole and two
// further allpasses, then the plate tank: two one-poles (decay filtering), a feedback allpass and
// two tank allpasses (one modulated, left), and four output taps. Outputs A = D, B = C. Defaults
// are the factory settings with Mode Enhancement at rest [R].
class Plate final : public Program {
public:
    struct Offsets {
        uint16_t inL = 20723, inR = 43129;              // input line (gain-scaled input)
        uint16_t inTapL = 20725, inTapR = 43131;        // its read point (PREDELAY moves it)
        uint16_t ap1L[2] = { 146, 202 }, ap1R[2] = { 1850, 1890 };      // {w, d}
        uint16_t lpL = 128, lpR = 130;                  // one-pole after the input allpass
        uint16_t ap2L[2] = { 206, 394 }, ap2R[2] = { 1894, 2026 };
        uint16_t ap3L[2] = { 398, 902 }, ap3R[2] = { 2030, 2386 };
        uint16_t xferIn = 7308, xferOut = 0;            // left transfer line into the modulated tap
        uint16_t crossL = 20716, crossR = 11844;        // tank cross-feed taps
        uint16_t damp1L = 132, damp1R = 134, damp2L = 136, damp2R = 138;   // tank one-poles
        uint16_t fbL[2] = { 3560, 4692 }, fbR[2] = { 12092, 13348 };    // feedback allpass {w, d}
        uint16_t modL[2] = { 0, 0 };                    // fractional tap (Mode Enhancement)
        uint16_t tank1L = 5944, tank1R[2] = { 14820, 16364 };
        uint16_t tank2L[2] = { 5772, 11008 }, tank2R[2] = { 14648, 19600 };
        uint16_t tankOutL = 7336, tankOutR = 13348;
        uint16_t mixL[3] = { 5452, 8180, 5604 }, mixR[3] = { 14188, 17336, 14480 };
        uint16_t outA[4] = { 1082, 4700, 16584, 11092 }, outB[4] = { 2386, 13436, 7432, 19964 };
        uint16_t echo[6] = { 20865, 43582, 22663, 43420, 21949, 45014 };
        uint16_t lastL = 8336, lastR = 17548;
    };
    struct Coefs {
        int in = 16;                                    // input gain
        int pre = 32;                                   // predelayed input into the first allpass
        int ap1G = 26, ap1K = 11;                       // DIFFUSION (three input allpasses)
        int lpFb = 8, lpIn = 24;                        // HF BANDWIDTH pair
        int ap2G = 23, ap2K = 15, ap3G = 13, ap3K = 27;
        int trebleIn = 30, trebleFb = 2;                // TREBLE DECAY pair (tank cross-feed one-pole)
        int xoIn = 6, xoFb = 26;                        // CROSSOVER pair
        int mid = 21, lfDiff = 0;                       // MID curve on the treble output, LF − MID on the crossover
        int fbG = 16, fbK = 24;                         // feedback allpass (DEFINITION group)
        int modW = 32;
        int t1G = 12, t1K = 27, t2G = 15, t2K = 25;     // tank allpasses (MID group)
        int mix1 = 25, mix2L = -23, mix2R = 23;         // MID curves on the tank mix
        int outA[4] = { -25, -16, 16, 16 }, outB[4] = { 25, -16, 16, 16 };   // DEPTH
        int echo[6] = { 0, 0, 0, 0, 0, 0 };             // pre-echo LEVELs
    };

    void tick(CoreState& s) override;
    int loopLength() const override { return 100; }
    void applyControls(const XlRegs& r) override;
    XlRegs factory() const override;
    void setDecayReduction(int steps) override {
        c.t1G = law::reduced(g1_, steps); c.t1K = law::allpassK(c.t1G);
        c.t2G = law::reduced(g2_, steps); c.t2K = law::allpassK(c.t2G);
        reduction_ = steps;
    }
    int modTaps() const override { return 1; }
    uint16_t modHome(int) const override { return 0; }
    void setModTap(int, uint16_t o0, uint16_t o1, int w) override { o.modL[0] = o0; o.modL[1] = o1; c.modW = w; }
    void predelayRamp(uint16_t*& l, uint16_t*& r, int*& g) override { l = &o.inTapL; r = &o.inTapR; g = &c.pre; }
    static Offsets templateOffsets();
    Offsets o;
    Coefs c;
private:
    int g1_ = 12, g2_ = 15, reduction_ = 0;
};

// The 224XL factory programs implemented so far: name, program id, algorithm, factory registers.
struct ProgramInfo {
    const char* name;
    uint8_t id;
    int algorithm;          // 0 CONCERT HALL network, 1 PLATE network
};
const ProgramInfo* xlPrograms(int& count);
const ProgramInfo* findXlProgram(const char* name);
std::unique_ptr<Program> makeAlgorithm(int algorithm);
XlRegs xlFactory(const ProgramInfo& p);

} // namespace tearwash

#endif
