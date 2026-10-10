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

// 224XL CHAMBER. Mono input (L and R summed); an input chain of four allpasses with a one-pole;
// two tank legs, each a fractional (Mode Enhancement) tap into two one-poles and a run of
// allpasses; outputs A and C from taps; B and D are the sum and difference of A and C (one and
// two samples late). SIZE does not apply. Defaults are the factory settings, taps at rest [R].
class Chamber final : public Program {
public:
    struct Offsets {
        uint16_t sumL = 20940, sumR = 20946, outCStore = 20943;   // stored mono input, out-A and out-C sums
        uint16_t monoIn = 20971;                       // mono input line
        uint16_t monoTap = 20973;                      // its read point (PREDELAY moves it)
        uint16_t ap1[2] = { 2047, 2284 };              // {w, d}
        uint16_t lp = 2285;                            // one-pole after the first allpass
        uint16_t ap2[2] = { 2286, 2929 };
        uint16_t ap3[2] = { 3785, 5832 }, ap4[2] = { 3957, 5184 };
        uint16_t bdA = 20947, bdC = 20944;             // taps for the B/D matrix
        uint16_t tA1[2] = { 4126, 4912 }, tA2[2] = { 7029, 11077 }, tA3[2] = { 7370, 10217 };
        uint16_t modA[2] = { 11441, 11443 }, modB[2] = { 20178, 20180 };
        uint16_t lp1A = 20959, lp2A = 20956, lp1B = 20953, lp2B = 20950;
        uint16_t feed = 2932;                          // input-chain output into both legs
        uint16_t outA[4] = { 6345, 15081, 11601, 20337 }, storeA = 11669;
        uint16_t inB1[2] = { 3616, 6242 }, inB2[2] = { 11737, 14809 };
        uint16_t storeB = 2933;
        uint16_t tB1[2] = { 11822, 14246 }, tB2[2] = { 11991, 13391 }, tB3[2] = { 12162, 12538 };
        uint16_t tB4[2] = { 15748, 19332 }, tB5[2] = { 16089, 18443 };
        uint16_t outC[4] = { 15731, 6995, 19892, 11156 };
    };
    struct Coefs {
        int monoL = 32, monoR = 12, monoSum = 12;      // input gains (left path, mono sum)
        int pre = 32;                                  // predelayed input
        int ap1G = 16, ap1K = 24, ap2G = 16, ap2K = 24; // DIFFUSION
        int lpFb = 6, lpIn = 26;                       // HF BANDWIDTH pair
        int ap3G = 8, ap3K = 30, ap4G = 8, ap4K = 30;
        int tA1G = 8, tA1K = 30, tA2G = 11, tA2K = 28, tA3G = 13, tA3K = 27;
        int modAw = 32, modBw = 32;
        int lp1In = 28, lp1Fb = 4;                     // TREBLE DECAY pair
        int lp2In = 6, lp2Fb = 26;                     // CROSSOVER pair
        int loop = 12, feed = 32, lpMix = 3;           // MID loop gain, input feed, LF − MID on the crossover
        int outA[4] = { 30, -15, 5, -3 }, outC[4] = { 30, -15, 5, -3 };   // DEPTH
        int inB1G = 10, inB1K = 29, inB2G = 10, inB2K = 29;   // MID group (with tA2, tB4)
        int tB1G = 8, tB1K = 30, tB2G = 8, tB2K = 30, tB3G = 8, tB3K = 30, tB4G = 11, tB4K = 28, tB5G = 13, tB5K = 27;
    };

    void tick(CoreState& s) override;
    int loopLength() const override { return 100; }
    int modTaps() const override { return 2; }
    uint16_t modHome(int i) const override { return i ? 20178 : 11441; }
    void setModTap(int i, uint16_t o0, uint16_t o1, int w) override {
        uint16_t* t = i ? o.modB : o.modA;
        t[0] = o0; t[1] = o1;
        (i ? c.modBw : c.modAw) = w;
    }
    void applyControls(const XlRegs& r) override;
    XlRegs factory() const override;
    void predelayRamp(uint16_t*& l, uint16_t*& r, int*& g) override { l = &o.monoTap; r = &o.monoTap; g = &c.pre; }
    void setDecayReduction(int steps) override {
        reduction_ = steps;
        c.tA2G = law::reduced(g11_, steps); c.tA2K = law::allpassK(c.tA2G);
        c.tB4G = law::reduced(g11_, steps); c.tB4K = law::allpassK(c.tB4G);
        c.inB1G = law::reduced(g10_, steps); c.inB1K = law::allpassK(c.inB1G);
        c.inB2G = law::reduced(g10_, steps); c.inB2K = law::allpassK(c.inB2G);
    }
    Offsets o;
    Coefs c;
private:
    int g11_ = 11, g10_ = 10, reduction_ = 0;
};

// The 224XL factory programs implemented so far: name, program id, algorithm, factory registers.
struct ProgramInfo {
    const char* name;
    uint8_t id;
    int algorithm;          // 0 CONCERT HALL network, 1 PLATE network, 2 CHAMBER network
};
const ProgramInfo* xlPrograms(int& count);
const ProgramInfo* findXlProgram(const char* name);
std::unique_ptr<Program> makeAlgorithm(int algorithm);
XlRegs xlFactory(const ProgramInfo& p);

} // namespace tearwash

#endif
