// 224XL algorithm networks (docs/tearwash/03 §1). Statement order is evaluation order: every
// tap is read and every store made at the point the original program does, so delay timing
// within a sample, rounding and saturation match it [R]. Comments name the sections.

#include "Programs.hpp"

#include <algorithm>
#include <string>

namespace tearwash {

void ConcertHall::tick(CoreState& s) {
    Pipe p{ s, a_, r_ };
    Mac& a = a_;
    int16_t x, d, t;

    // ---- Left half --------------------------------------------------------------------------
    // Input bandwidth one-pole: lp = A·in + B·lp[-1]; the previous sample's last sum is stored.
    p.next();
    a.add(s.inL, c.hfIn);
    p.store(o.carry, r_);
    a.add(p.tap(o.lpL + 1), c.hfFb);
    // Four input diffusion allpasses; each stores the previous stage's output.
    p.allpass(o.apL[0][0], o.apL[0][1], o.lpL, o.apL[0][0], c.apG[0], c.apK[0]);
    p.allpass(o.apL[1][0], o.apL[1][1], o.apL[0][1], o.apL[1][0], c.apG[1], c.apK[1]);
    p.allpass(o.apL[2][0], o.apL[2][1], o.apL[1][1], o.apL[2][0], c.apG[2], c.apK[2]);
    p.allpass(o.apL[3][0], o.apL[3][1], o.apL[2][1], o.apL[3][0], c.apG[3], c.apK[3]);
    // Fractional tank tap (two adjacent taps, weights w and 32 - w).
    t = p.tap(o.modL[0]);
    p.next();
    a.add(t, c.modLw);
    a.add(p.tap(o.modL[1]), 32 - c.modLw);
    p.store(o.apL[3][1], r_);
    // Modulated tank allpass: x from the tank line, d = the fractional tap.
    x = p.tap(o.tankL);
    p.next();
    a.add(x, 32);
    d = r_;
    a.add(d, c.tankG);
    p.next();
    a.add(d, c.tankK);
    p.store(o.tankL, r_);
    a.add(x, -c.tankG);
    // Output B taps.
    t = p.tap(o.outB1);
    p.next();
    a.add(t, c.outB1);
    a.add(p.tap(o.outB2), c.outB2);
    p.store(o.tankOutL, r_);
    // Crossover one-pole; output B. The loop gain is MID on the full band plus (LF − MID) on
    // the crossover's low band; the predelayed input joins here.
    x = p.tap(o.xoInL);
    p.next();
    a.add(x, c.xoIn);
    a.add(p.tap(o.xoL + 1), c.xoFb);
    s.dac[1] = r_;
    p.next();
    a.add(x, c.mid);
    p.store(o.xoL, r_);
    a.add(r_, c.lfDiff);
    a.add(p.tap(o.preL), c.pre);
    // Treble one-pole (continues into the first diffusion allpass without clearing).
    t = p.tap(o.trebleL + 1);
    p.next();
    a.add(t, c.trebleFb);
    a.add(r_, c.trebleIn);
    d = p.tap(o.dif1L[1]);
    r_ = a.result();
    a.add(d, c.dif1G);
    p.store(o.trebleL, r_);
    x = r_;
    p.next();
    a.add(d, c.dif1K);
    p.store(o.dif1L[0], r_);
    a.add(x, -c.dif1G);
    p.allpass(o.dif2L[0], o.dif2L[1], o.dif1L[1], o.dif2L[0], c.dif2G, c.dif2K);
    // Output A taps.
    t = p.tap(o.outA[0]);
    p.next();
    a.add(t, c.outA[0]);
    p.store(o.decOutL, r_);
    a.add(r_, c.outA[1]);
    a.add(p.tap(o.outA[1]), c.outA[2]);
    a.add(p.tap(o.outA[2]), c.outA[3]);
    a.add(p.tap(o.echo[0]), c.echo[0]);
    a.add(p.tap(o.echo[2]), c.echo[2]);
    t = p.tap(o.xferA);
    p.next();
    a.add(t, 32);
    s.dac[0] = r_;
    r_ = a.result();
    p.store(o.xferAOut, r_);

    // ---- Right half -------------------------------------------------------------------------
    p.next();
    a.add(s.inR, c.hfIn);
    a.add(p.tap(o.lpR + 1), c.hfFb);
    p.allpass(o.apR[0][0], o.apR[0][1], o.lpR, o.apR[0][0], c.apG[0], c.apK[0]);
    p.allpass(o.apR[1][0], o.apR[1][1], o.apR[0][1], o.apR[1][0], c.apG[1], c.apK[1]);
    p.allpass(o.apR[2][0], o.apR[2][1], o.apR[1][1], o.apR[2][0], c.apG[2], c.apK[2]);
    p.allpass(o.apR[3][0], o.apR[3][1], o.apR[2][1], o.apR[3][0], c.apG[3], c.apK[3]);
    t = p.tap(o.modR[0]);
    p.next();
    a.add(t, c.modRw);
    a.add(p.tap(o.modR[1]), 32 - c.modRw);
    p.store(o.apR[3][1], r_);
    x = p.tap(o.tankR);
    p.next();
    a.add(x, 32);
    d = r_;
    a.add(d, c.tankG);
    p.next();
    a.add(d, c.tankK);
    p.store(o.tankR, r_);
    a.add(x, -c.tankG);
    t = p.tap(o.outD1);
    p.next();
    a.add(t, c.outD1);
    p.store(o.tankOutR, r_);
    x = p.tap(o.xoInR);
    p.next();
    a.add(x, -c.xoIn);
    a.add(p.tap(o.xoR + 1), c.xoFb);
    s.dac[3] = r_;
    p.next();
    a.add(x, -c.mid);
    p.store(o.xoR, r_);
    a.add(r_, c.lfDiff);
    a.add(p.tap(o.preR), c.pre);
    t = p.tap(o.trebleR + 1);
    p.next();
    a.add(t, c.trebleFb);
    a.add(r_, c.trebleIn);
    d = p.tap(o.dif1R[1]);
    r_ = a.result();
    a.add(d, c.dif1G);
    p.store(o.trebleR, r_);
    x = r_;
    p.next();
    a.add(d, c.dif1K);
    p.store(o.dif1R[0], r_);
    a.add(x, -c.dif1G);
    p.allpass(o.dif2R[0], o.dif2R[1], o.dif1R[1], o.dif2R[0], c.dif2G, c.dif2K);
    // Output C taps.
    t = p.tap(o.outC[0]);
    p.next();
    a.add(t, c.outC[0]);
    a.add(p.tap(o.outC[1]), c.outC[1]);
    a.add(p.tap(o.outC[2]), c.outC[2]);
    a.add(p.tap(o.outC[3]), c.outC[3]);
    a.add(p.tap(o.echo[1]), c.echo[1]);
    a.add(p.tap(o.echo[3]), c.echo[3]);
    p.store(o.decOutR, r_);
    t = p.tap(o.xferC);
    p.next();
    a.add(t, 32);
    s.dac[2] = r_;

    s.mem.advance();
}

// Offsets at the smallest SIZE; SIZE maps them (03 §3) [R].
ConcertHall::Offsets ConcertHall::templateOffsets() {
    Offsets t;
    t.carry = 128; t.lpL = 6006; t.lpR = 35770;
    const uint16_t apL[4][2] = { { 1756, 2242 }, { 1714, 2395 }, { 2481, 3454 }, { 2524, 3138 } };
    const uint16_t apR[4][2] = { { 4255, 4763 }, { 4212, 4855 }, { 4980, 6000 }, { 5023, 5665 } };
    for (int i = 0; i < 4; ++i) for (int k = 0; k < 2; ++k) { t.apL[i][k] = apL[i][k]; t.apR[i][k] = apR[i][k]; }
    t.tankL = 2567; t.tankR = 5066; t.tankOutL = 2934; t.tankOutR = 5493;
    t.outB1 = 4861; t.outB2 = 3463; t.outD1 = 2464;
    t.xoInL = 6004; t.xoInR = 3847; t.xoL = 258; t.xoR = 256;
    t.preL = 6007; t.preR = 35771; t.trebleL = 260; t.trebleR = 262;
    t.dif1L[0] = 277; t.dif1L[1] = 516; t.dif1R[0] = 911; t.dif1R[1] = 1116;
    t.dif2L[0] = 518; t.dif2L[1] = 910; t.dif2R[0] = 1117; t.dif2R[1] = 1446;
    t.decOutL = 1447; t.decOutR = 3847;
    t.outA[0] = 2405; t.outA[1] = 4903; t.outA[2] = 3591;
    t.outC[0] = 4972; t.outC[1] = 3921; t.outC[2] = 2473; t.outC[3] = 6002;
    t.echo[0] = 6007; t.echo[1] = 35771; t.echo[2] = 35771; t.echo[3] = 6007;
    t.xferA = 5429; t.xferC = 2870; t.xferAOut = 0;
    return t;
}

static law::SizeMap concertHallSize() {
    law::SizeMap m;
    m.active = true;
    m.t0 = 0x05A7; m.t1 = 0x1774; m.t2 = 0x05A7; m.t3 = 0x05A7; m.f = 0x40; m.lo = 0x0A; m.hi = 0x29;
    return m;
}

XlRegs ConcertHall::factory() const {
    XlRegs r;
    r.size = 0xFE;
    const uint8_t p1[6] = { 0x84, 0x5F, 0x20, 0xB6, 0x55, 0x00 };   // LF, MID, XOVER, TREBLE, DEPTH, PREDELAY
    const uint8_t p3[6] = { 0x84, 0x84, 0x80, 0xD0, 0x40, 0x02 };   // LF/MID STOP, CHORUS, HF BW, DIFFUSION, DEFINITION
    const uint8_t p4[6] = { 0x02, 0x02, 0x02, 0x02, 0x00, 0x00 };   // pre-echo LEVELs
    const uint8_t p5[6] = { 0x05, 0x09, 0x11, 0x19, 0x02, 0x52 };   // pre-echo DELAYs
    const uint8_t p6[6] = { 0x00, 0xC0, 0x80, 0x00, 0xF0, 0x10 };   // FINE delays
    for (int i = 0; i < 6; ++i) {
        r.page[0][i] = p1[i]; r.page[2][i] = p3[i]; r.page[3][i] = p4[i]; r.page[4][i] = p5[i]; r.page[5][i] = p6[i];
    }
    r.options = 0xC0;
    return r;
}

// Control laws of CONCERT HALL (03 §3) [R].
void ConcertHall::applyControls(const XlRegs& r) {
    using namespace law;
    // Register clamps applied by the controller: decays ≤ F9, CROSSOVER ≥ 08, DEFINITION ≤ C0.
    const uint8_t lf = r.at(1, 1) > 0xF9 ? 0xF9 : r.at(1, 1);
    const uint8_t mid = r.at(1, 2) > 0xF9 ? 0xF9 : r.at(1, 2);
    const uint8_t xo = r.at(1, 3) < 0x08 ? 0x08 : r.at(1, 3);
    const uint8_t def = r.at(3, 6) > 0xC0 ? 0xC0 : r.at(3, 6);

    pair(r.at(3, 4), c.hfIn, c.hfFb);
    pair(xo, c.xoIn, c.xoFb);
    pair(r.at(1, 4), c.trebleIn, c.trebleFb);
    c.mid = step5(mid);
    c.lfDiff = step5(lf) - step5(mid);

    // DEFINITION caps the MID-group allpasses through s(MID) and sets the DEFINITION group.
    const int defCap = ((0xFF - def) >> 3) + 1;
    const int sMid = step5(mid) < defCap - 1 ? step5(mid) : defCap - 1;
    int gMid = (5 * sMid) / 4;
    if (gMid > 16) gMid = 16;
    if (gMid < 0) gMid = 0;
    int gDef = ((2 * ((0xFF - def) >> 2)) / 4) / 2;
    if (gDef > 8) gDef = 8;
    c.apG[0] = c.apG[3] = c.tankG = gDef;
    c.apK[0] = c.apK[3] = c.tankK = allpassK(gDef);
    gMid_ = gMid;
    setDecayReduction(reduction_);

    // DIFFUSION: the two allpasses in the decay section.
    const int x = r.at(3, 5) >> 2;
    c.dif1G = std::min(26, (6 * x / 4) / 2);
    c.dif2G = std::min(26, (5 * x / 4) / 2);
    c.dif1K = allpassK(c.dif1G);
    c.dif2K = allpassK(c.dif2G);

    // DEPTH: four output-tap curves.
    static const int kD0[4] = { 10, 15, 20, 0 }, kD1[4] = { 31, 29, 18, 0 }, kD3[4] = { 1, 5, 15, 31 };
    const uint8_t dep = r.at(1, 5);
    c.outA[0] = c.outC[0] = curve4(kD0, dep);
    c.outA[1] = c.outC[1] = curve4(kD1, dep);
    c.outA[2] = c.outC[2] = -curve4(kD0, dep);
    c.outA[3] = c.outC[3] = curve4(kD3, dep);

    // SIZE maps every offset; the delay builders then add their delays to S(base) + 1.
    law::SizeMap sm = concertHallSize();
    sm.set(r.size);
    const Offsets t = templateOffsets();
    const uint16_t* src = reinterpret_cast<const uint16_t*>(&t);
    uint16_t* dst = reinterpret_cast<uint16_t*>(&o);
    uint16_t keepMod[4] = { o.modL[0], o.modL[1], o.modR[0], o.modR[1] };
    for (size_t i = 0; i < sizeof(Offsets) / sizeof(uint16_t); ++i) dst[i] = sm(src[i]);
    o.modL[0] = keepMod[0]; o.modL[1] = keepMod[1]; o.modR[0] = keepMod[2]; o.modR[1] = keepMod[3];
    auto base = [&](uint16_t x) { return static_cast<int>(sm(x)) + 1; };

    // PREDELAY (millisecond law, 34 samples per ms; the register is clamped to fit the map).
    const int pd = 34 * predelayMs(clampPredelay(r.at(1, 6), sm.delayLimit()));
    o.preL = static_cast<uint16_t>(base(t.preL) + pd);
    o.preR = static_cast<uint16_t>(base(t.preR) + pd);

    // Pre-echoes: LEVELs, DELAYs (34 samples per step) with FINE; the decay taps (4 per step).
    for (int i = 0; i < 4; ++i) c.echo[i] = r.at(4, i + 1) >> 2;
    o.echo[0] = static_cast<uint16_t>(base(t.echo[0]) + fineDelay(r.at(5, 1), r.at(6, 1), 34));
    o.echo[1] = static_cast<uint16_t>(base(t.echo[1]) + fineDelay(r.at(5, 2), r.at(6, 2), 34));
    o.echo[2] = static_cast<uint16_t>(base(t.echo[2]) + fineDelay(r.at(5, 3), r.at(6, 3), 34));
    o.echo[3] = static_cast<uint16_t>(base(t.echo[3]) + fineDelay(r.at(5, 4), r.at(6, 4), 34));
    o.decOutL = static_cast<uint16_t>(base(t.decOutL) + fineDelay(r.at(5, 5), r.at(6, 5), 4));
    o.outC[1] = static_cast<uint16_t>(base(t.outC[1]) + fineDelay(r.at(5, 6), r.at(6, 6), 4));
}

} // namespace tearwash

namespace tearwash {

void Plate::tick(CoreState& s) {
    Pipe p{ s, a_, r_ };
    Mac& a = a_;
    int16_t x, d, t, held;

    // ---- Left half --------------------------------------------------------------------------
    // Input gain, then the input allpass (x from the input line).
    p.next();
    a.add(s.inL, c.in);
    x = p.tap(o.inTapL);
    p.next();
    a.add(x, c.pre);
    d = p.tap(o.ap1L[1]);
    a.add(d, c.ap1G);
    p.store(o.inL, r_);
    p.next();
    a.add(d, c.ap1K);
    p.store(o.ap1L[0], r_);
    a.add(x, -c.ap1G);
    // One-pole, continuing into the second allpass.
    x = p.tap(o.lpL + 1);
    p.next();
    a.add(x, c.lpFb);
    a.add(r_, c.lpIn);
    d = p.tap(o.ap2L[1]);
    r_ = a.result();
    a.add(d, c.ap2G);
    p.store(o.lpL, r_);
    x = r_;
    p.next();
    a.add(d, c.ap2K);
    p.store(o.ap2L[0], r_);
    a.add(x, -c.ap2G);
    p.allpass(o.ap3L[0], o.ap3L[1], o.ap2L[1], o.ap3L[0], c.ap3G, c.ap3K);
    // Transfer tap, store the input section's output.
    t = p.tap(o.xferIn);
    p.next();
    a.add(t, 32);
    p.store(o.ap3L[1], r_);
    // Tank: cross-feed into the first one-pole, the second one-pole.
    t = p.tap(o.crossL);
    p.next();
    a.add(t, c.trebleIn);
    d = p.tap(o.damp1L + 1);
    a.add(d, c.trebleFb);
    p.store(o.xferOut, r_);
    p.next();
    a.add(d, c.xoIn);
    a.add(p.tap(o.damp2L + 1), c.xoFb);
    p.store(o.damp1L, r_);
    p.next();
    a.add(d, c.mid);
    p.store(o.damp2L, r_);
    a.add(r_, c.lfDiff);
    held = p.tap(o.ap3L[1]);                // input section output, held for the mix below
    a.add(held, -32);
    // Feedback allpass.
    d = p.tap(o.fbL[1]);
    r_ = a.result();
    a.add(d, c.fbG);
    x = r_;
    p.next();
    a.add(d, c.fbK);
    p.store(o.fbL[0], r_);
    a.add(x, -c.fbG);
    // Fractional tap (Mode Enhancement) as d of the first tank allpass.
    t = p.tap(o.modL[0]);
    p.next();
    a.add(t, c.modW);
    a.add(p.tap(o.modL[1]), 32 - c.modW);
    p.store(o.fbL[1], r_);
    x = p.tap(o.tank1L);
    p.next();
    a.add(x, 32);
    d = r_;
    a.add(d, c.t1G);
    p.next();
    a.add(d, c.t1K);
    p.store(o.tank1L, r_);
    a.add(x, -c.t1G);
    p.allpass(o.tank2L[0], o.tank2L[1], o.tankOutL, o.tank2L[0], c.t2G, c.t2K);
    // Tank mix with the held input-section output.
    t = p.tap(o.mixL[0]);
    p.next();
    a.add(t, c.mix1);
    p.store(o.tank2L[1], r_);
    a.add(held, 32);
    t = p.tap(o.mixL[1]);
    p.next();
    a.add(t, c.mix2L);
    p.store(o.mixL[2], r_);
    a.add(held, 32);
    // Output A (= D) taps.
    t = p.tap(o.outA[0]);
    p.next();
    a.add(t, c.outA[0]);
    a.add(p.tap(o.outA[1]), c.outA[1]);
    a.add(p.tap(o.outA[2]), c.outA[2]);
    a.add(p.tap(o.outA[3]), c.outA[3]);
    a.add(p.tap(o.echo[0]), c.echo[0]);
    a.add(p.tap(o.echo[1]), c.echo[1]);
    a.add(p.tap(o.echo[2]), c.echo[2]);
    p.store(o.lastL, r_);
    r_ = a.result();
    s.dac[0] = s.dac[3] = r_;
    r_ = a.result();

    // ---- Right half -------------------------------------------------------------------------
    p.next();
    a.add(s.inR, c.in);
    x = p.tap(o.inTapR);
    p.next();
    a.add(x, c.pre);
    d = p.tap(o.ap1R[1]);
    a.add(d, c.ap1G);
    p.store(o.inR, r_);
    p.next();
    a.add(d, c.ap1K);
    p.store(o.ap1R[0], r_);
    a.add(x, -c.ap1G);
    x = p.tap(o.lpR + 1);
    p.next();
    a.add(x, c.lpFb);
    a.add(r_, c.lpIn);
    d = p.tap(o.ap2R[1]);
    r_ = a.result();
    a.add(d, c.ap2G);
    p.store(o.lpR, r_);
    x = r_;
    p.next();
    a.add(d, c.ap2K);
    p.store(o.ap2R[0], r_);
    a.add(x, -c.ap2G);
    p.allpass(o.ap3R[0], o.ap3R[1], o.ap2R[1], o.ap3R[0], c.ap3G, c.ap3K);
    t = p.tap(o.crossR);
    p.next();
    a.add(t, c.trebleIn);
    d = p.tap(o.damp1R + 1);
    a.add(d, c.trebleFb);
    p.store(o.ap3R[1], r_);
    p.next();
    a.add(d, c.xoIn);
    a.add(p.tap(o.damp2R + 1), c.xoFb);
    p.store(o.damp1R, r_);
    p.next();
    a.add(d, c.mid);
    p.store(o.damp2R, r_);
    a.add(r_, c.lfDiff);
    a.add(p.tap(o.ap3R[1]), -32);
    d = p.tap(o.fbR[1]);
    r_ = a.result();
    a.add(d, c.fbG);
    x = r_;
    p.next();
    a.add(d, c.fbK);
    p.store(o.fbR[0], r_);
    a.add(x, -c.fbG);
    p.allpass(o.tank1R[0], o.tank1R[1], o.fbR[1], o.tank1R[0], c.t1G, c.t1K);
    p.allpass(o.tank2R[0], o.tank2R[1], o.tank1R[1], o.tank2R[0], c.t2G, c.t2K);
    t = p.tap(o.mixR[0]);
    p.next();
    a.add(t, c.mix1);
    p.store(o.tank2R[1], r_);
    a.add(p.tap(o.ap3R[1]), 32);
    t = p.tap(o.mixR[1]);
    p.next();
    a.add(t, c.mix2R);
    p.store(o.mixR[2], r_);
    a.add(p.tap(o.ap3R[1]), 32);
    t = p.tap(o.ap3R[1]);
    p.next();
    a.add(t, c.outB[0]);
    a.add(p.tap(o.outB[1]), c.outB[1]);
    a.add(p.tap(o.outB[2]), c.outB[2]);
    a.add(p.tap(o.outB[3]), c.outB[3]);
    a.add(p.tap(o.echo[3]), c.echo[3]);
    a.add(p.tap(o.echo[4]), c.echo[4]);
    a.add(p.tap(o.echo[5]), c.echo[5]);
    p.store(o.lastR, r_);
    r_ = a.result();
    s.dac[1] = s.dac[2] = r_;

    s.mem.advance();
}

} // namespace tearwash

namespace tearwash {

Plate::Offsets Plate::templateOffsets() {
    Offsets t;
    t.inL = 7852; t.inR = 36694; t.inTapL = 7853; t.inTapR = 36695;
    t.ap1L[0] = 146; t.ap1L[1] = 160; t.ap1R[0] = 572; t.ap1R[1] = 582;
    t.lpL = 128; t.lpR = 130;
    t.ap2L[0] = 161; t.ap2L[1] = 208; t.ap2R[0] = 583; t.ap2R[1] = 616;
    t.ap3L[0] = 209; t.ap3L[1] = 335; t.ap3R[0] = 617; t.ap3R[1] = 706;
    t.xferIn = 4497; t.xferOut = 0; t.crossL = 7849; t.crossR = 5631;
    t.damp1L = 132; t.damp1R = 134; t.damp2L = 136; t.damp2R = 138;
    t.fbL[0] = 3560; t.fbL[1] = 3843; t.fbR[0] = 5693; t.fbR[1] = 6007;
    t.modL[0] = t.modL[1] = 0;
    t.tank1L = 4156; t.tank1R[0] = 6375; t.tank1R[1] = 6761;
    t.tank2L[0] = 4113; t.tank2L[1] = 5422; t.tank2R[0] = 6332; t.tank2R[1] = 7570;
    t.tankOutL = 4504; t.tankOutR = 6007;
    t.mixL[0] = 4033; t.mixL[1] = 4715; t.mixL[2] = 4071; t.mixR[0] = 6217; t.mixR[1] = 7004; t.mixR[2] = 6290;
    t.outA[0] = 380; t.outA[1] = 3845; t.outA[2] = 6816; t.outA[3] = 5443;
    t.outB[0] = 706; t.outB[1] = 6029; t.outB[2] = 4528; t.outB[3] = 7661;
    for (int i = 0; i < 6; ++i) t.echo[i] = (i == 0 || i == 2 || i == 4) ? 7853 : 36695;
    t.lastL = 4754; t.lastR = 7057;
    return t;
}

static law::SizeMap plateSize() {
    law::SizeMap m;
    m.active = true;
    m.t0 = 0x0DE8; m.t1 = 0x1EAA; m.t2 = 0x0092; m.t3 = 0x02C2; m.f = 0x40; m.lo = 0x0A; m.hi = 0x29;
    return m;
}

XlRegs Plate::factory() const {
    XlRegs r;
    r.size = 0xFE;
    const uint8_t p1[6] = { 0x55, 0x55, 0x31, 0xF6, 0x00, 0x00 };
    const uint8_t p3[6] = { 0x89, 0x89, 0x80, 0xC2, 0x95, 0x02 };
    const uint8_t p4[6] = { 0x02, 0x02, 0x02, 0x02, 0x02, 0x02 };
    const uint8_t p5[6] = { 0x04, 0x08, 0x0D, 0x24, 0x39, 0x37 };
    const uint8_t p6[6] = { 0x20, 0x80, 0x40, 0x00, 0x00, 0x60 };
    for (int i = 0; i < 6; ++i) {
        r.page[0][i] = p1[i]; r.page[2][i] = p3[i]; r.page[3][i] = p4[i]; r.page[4][i] = p5[i]; r.page[5][i] = p6[i];
    }
    r.options = 0xC0;
    return r;
}

// Control laws of PLATE (03 §3) [R].
void Plate::applyControls(const XlRegs& r) {
    using namespace law;
    const uint8_t lf = r.at(1, 1) > 0xF9 ? 0xF9 : r.at(1, 1);
    const uint8_t mid = r.at(1, 2) > 0xF9 ? 0xF9 : r.at(1, 2);
    const uint8_t xo = r.at(1, 3) < 0x08 ? 0x08 : r.at(1, 3);
    const uint8_t def = r.at(3, 6) > 0xC0 ? 0xC0 : r.at(3, 6);
    const int sMid = step5(mid);

    pair(r.at(3, 4), c.lpIn, c.lpFb);
    pair(r.at(1, 4), c.trebleIn, c.trebleFb);
    pair(xo, c.xoIn, c.xoFb);
    // MID DECAY through rows of the decay-curve table; LF − MID on the crossover output.
    static const int kRowLoop[5] = { 22, 41, 52, 59, 64 }, kRowMix1[5] = { 38, 48, 58, 62, 65 },
                     kRowMix2[5] = { 28, 45, 55, 60, 65 };
    c.mid = decayCurve(kRowLoop, sMid);
    c.lfDiff = step5(lf) - sMid;
    c.mix1 = decayCurve(kRowMix1, sMid);
    c.mix2L = -decayCurve(kRowMix2, sMid);
    c.mix2R = decayCurve(kRowMix2, sMid);

    // DEFINITION: the feedback allpasses; it also caps s(MID) for the tank allpasses.
    const int defX = (0xFF - def) >> 2;
    const int defCap = ((0xFF - def) >> 3) + 1;
    c.fbG = scaledGain(5, 16, defX, 2);
    c.fbK = allpassK(c.fbG);
    const int sCap = sMid < defCap - 1 ? sMid : defCap - 1;
    g1_ = scaledGain(5, 16, sCap, 1);
    g2_ = scaledGain(6, 19, sCap, 1);
    setDecayReduction(reduction_);

    // DIFFUSION: the three input allpasses.
    const int x = r.at(3, 5) >> 2;
    c.ap1G = scaledGain(6, 26, x, 2); c.ap1K = allpassK(c.ap1G);
    c.ap2G = scaledGain(5, 26, x, 2); c.ap2K = allpassK(c.ap2G);
    c.ap3G = scaledGain(3, 31, x, 2); c.ap3K = allpassK(c.ap3G);

    // DEPTH: four output taps; A and B differ in the sign of the first.
    static const int kD0[4] = { 25, 16, 7, 3 }, kD1[4] = { 16, 16, 16, 10 }, kD3[4] = { 16, 16, 16, 26 };
    const uint8_t dep = r.at(1, 5);
    c.outA[0] = -curve4(kD0, dep);
    c.outB[0] = curve4(kD0, dep);
    c.outA[1] = c.outB[1] = -curve4(kD1, dep);
    c.outA[2] = c.outB[2] = 16;
    c.outA[3] = c.outB[3] = curve4(kD3, dep);

    // SIZE maps every offset; the delay builders add to S(base) + 1.
    law::SizeMap sm = plateSize();
    sm.set(r.size);
    const Offsets t = templateOffsets();
    const uint16_t* src = reinterpret_cast<const uint16_t*>(&t);
    uint16_t* dst = reinterpret_cast<uint16_t*>(&o);
    const uint16_t keepMod[2] = { o.modL[0], o.modL[1] };
    for (size_t i = 0; i < sizeof(Offsets) / sizeof(uint16_t); ++i) dst[i] = sm(src[i]);
    o.modL[0] = keepMod[0]; o.modL[1] = keepMod[1];
    auto base = [&](uint16_t x) { return static_cast<int>(sm(x)) + 1; };

    // PREDELAY moves the input line's read point (register clamped to fit the map).
    const int pd = 34 * predelayMs(clampPredelay(r.at(1, 6), sm.delayLimit()));
    o.inTapL = static_cast<uint16_t>(base(t.inTapL) + pd);
    o.inTapR = static_cast<uint16_t>(base(t.inTapR) + pd);

    // Pre-echoes: taps 1, 3, 5 feed A, 2, 4, 6 feed B; taps 1, 4, 5 read the left input line,
    // 2, 3, 6 the right (3 and 4 cross over).
    static const int kTapOf[6] = { 0, 3, 1, 4, 2, 5 };       // slider i -> echo slot
    static const bool kLeftLine[6] = { true, false, false, true, true, false };
    for (int i = 0; i < 6; ++i) {
        const int slot = kTapOf[i];
        const bool left = kLeftLine[i];
        c.echo[slot] = r.at(4, i + 1) >> 2;
        o.echo[slot] = static_cast<uint16_t>(base(left ? t.inTapL : t.inTapR) + fineDelay(r.at(5, i + 1), r.at(6, i + 1), 34));
    }
}

} // namespace tearwash

namespace tearwash {

namespace {
const ProgramInfo kXl[] = {
    { "CONCERT HALL", 0x01, 0 },
    { "ROOM", 0x04, 0 },
    { "PLATE", 0x02, 1 },
    { "SMALL PLATE", 0x03, 1 },
    { "CHAMBER", 0x08, 2 },
};

void setPages(XlRegs& r, const uint8_t (&p1)[6], const uint8_t (&p3)[6], const uint8_t (&p4)[6], const uint8_t (&p5)[6],
              const uint8_t (&p6)[6]) {
    for (int i = 0; i < 6; ++i) {
        r.page[0][i] = p1[i]; r.page[2][i] = p3[i]; r.page[3][i] = p4[i]; r.page[4][i] = p5[i]; r.page[5][i] = p6[i];
    }
}
} // namespace

const ProgramInfo* xlPrograms(int& count) {
    count = static_cast<int>(sizeof kXl / sizeof kXl[0]);
    return kXl;
}

const ProgramInfo* findXlProgram(const char* name) {
    for (const auto& p : kXl) if (std::string(p.name) == name) return &p;
    return nullptr;
}

std::unique_ptr<Program> makeAlgorithm(int algorithm) {
    if (algorithm == 1) return std::make_unique<Plate>();
    if (algorithm == 2) return std::make_unique<Chamber>();
    return std::make_unique<ConcertHall>();
}

// Factory registers (the preset values the original loads with each program).
XlRegs xlFactory(const ProgramInfo& p) {
    XlRegs r;
    switch (p.id) {
    case 0x04: {   // ROOM: the CONCERT HALL algorithm, smaller and brighter
        const uint8_t p1[6] = { 0x78, 0x78, 0x2B, 0xC6, 0x57, 0x00 }, p3[6] = { 0x88, 0x88, 0x8A, 0xC0, 0x6D, 0x02 },
                      p4[6] = { 0x02, 0x02, 0x02, 0x02, 0x00, 0x00 }, p5[6] = { 0x06, 0x07, 0x0C, 0x10, 0x13, 0x16 },
                      p6[6] = { 0x80, 0x00, 0x0C, 0x00, 0x00, 0x00 };
        setPages(r, p1, p3, p4, p5, p6);
        r.size = 0x56;
        break;
    }
    case 0x03: {   // SMALL PLATE: the PLATE algorithm at a smaller size
        const uint8_t p1[6] = { 0x78, 0x78, 0x31, 0xF6, 0x00, 0x00 }, p3[6] = { 0xA0, 0xA0, 0x83, 0xC0, 0x95, 0x10 },
                      p4[6] = { 0x03, 0x03, 0x03, 0x03, 0x03, 0x03 }, p5[6] = { 0x04, 0x08, 0x0D, 0x24, 0x39, 0x37 },
                      p6[6] = { 0, 0, 0, 0, 0, 0 };
        setPages(r, p1, p3, p4, p5, p6);
        r.size = 0x74;
        break;
    }
    default:
        r = makeAlgorithm(p.algorithm)->factory();
        break;
    }
    r.options = 0xC0;
    return r;
}

} // namespace tearwash

namespace tearwash {

void Chamber::tick(CoreState& s) {
    Pipe p{ s, a_, r_ };
    Mac& a = a_;
    int16_t x, d, t, t2;

    // Left input path; the previous sample's out-C sum is stored for the B/D matrix.
    a.zero();
    a.add(s.inL, c.monoL);
    p.store(o.outCStore, r_);
    x = p.tap(o.monoTap);
    p.next();
    a.add(x, c.pre);
    d = p.tap(o.ap1[1]);
    a.add(d, c.ap1G);
    p.store(o.sumL, r_);
    p.next();
    a.add(d, c.ap1K);
    p.store(o.ap1[0], r_);
    a.add(x, -c.ap1G);
    // One-pole into the second allpass (its −g·x term uses the one-pole's previous output).
    x = p.tap(o.lp + 1);
    p.next();
    a.add(x, c.lpFb);
    a.add(r_, c.lpIn);
    d = p.tap(o.ap2[1]);
    r_ = a.result();
    a.add(d, c.ap2G);
    p.store(o.lp, r_);
    p.next();
    a.add(d, c.ap2K);
    p.store(o.ap2[0], r_);
    a.add(x, -c.ap2G);
    p.allpass(o.ap3[0], o.ap3[1], o.ap2[1], o.ap3[0], c.ap3G, c.ap3K);
    p.allpass(o.ap4[0], o.ap4[1], o.ap3[1], o.ap4[0], c.ap4G, c.ap4K);
    // B = A + C and D = A − C, from the stored sums.
    x = p.tap(o.bdA);
    p.next();
    a.add(x, 32);
    t = p.tap(o.bdC);
    a.add(t, 32);
    p.store(o.ap4[1], r_);
    p.next();
    a.add(x, 32);
    s.dac[1] = r_;
    a.add(t, -32);
    // Tank leg A.
    x = p.tap(o.tA1[0]);
    p.next();
    a.add(x, 32);
    d = p.tap(o.tA1[1]);
    a.add(d, c.tA1G);
    s.dac[3] = r_;
    p.next();
    a.add(d, c.tA1K);
    p.store(o.tA1[0], r_);
    a.add(x, -c.tA1G);
    p.allpass(o.tA2[0], o.tA2[1], o.tA1[1], o.tA2[0], c.tA2G, c.tA2K);
    p.allpass(o.tA3[0], o.tA3[1], o.tA2[1], o.tA3[0], c.tA3G, c.tA3K);
    t = p.tap(o.modA[0]);
    p.next();
    a.add(t, c.modAw);
    a.add(p.tap(o.modA[1]), 32 - c.modAw);
    p.store(o.tA3[1], r_);
    r_ = a.result();
    t = r_;
    p.next();
    a.add(t, c.lp1In);
    d = p.tap(o.lp1A + 1);
    a.add(d, c.lp1Fb);
    p.next();
    a.add(d, c.lp2In);
    x = p.tap(o.lp2A + 1);
    a.add(x, c.lp2Fb);
    p.store(o.lp1A, r_);
    p.next();
    a.add(d, c.loop);
    a.add(p.tap(o.feed), c.feed);
    p.store(o.lp2A, r_);
    a.add(x, c.lpMix);
    t = p.tap(o.outA[0]);
    p.next();
    a.add(t, c.outA[0]);
    a.add(p.tap(o.outA[1]), c.outA[1]);
    a.add(p.tap(o.outA[2]), c.outA[2]);
    a.add(p.tap(o.outA[3]), c.outA[3]);
    p.store(o.storeA, r_);
    r_ = a.result();
    s.dac[0] = r_;

    // Mono sum, the right input chain, tank leg B.
    a.zero();
    a.add(s.inR, c.monoR);
    a.add(p.tap(o.sumL), c.monoSum);
    p.store(o.sumR, r_);
    x = p.tap(o.inB1[0]);
    p.next();
    a.add(x, 32);
    d = p.tap(o.inB1[1]);
    a.add(d, c.inB1G);
    p.store(o.monoIn, r_);
    p.next();
    a.add(d, c.inB1K);
    p.store(o.inB1[0], r_);
    a.add(x, -c.inB1G);
    p.allpass(o.inB2[0], o.inB2[1], o.inB1[1], o.inB2[0], c.inB2G, c.inB2K);
    t = p.tap(o.modB[0]);
    p.next();
    a.add(t, c.modBw);
    a.add(p.tap(o.modB[1]), 32 - c.modBw);
    p.store(o.inB2[1], r_);
    r_ = a.result();
    t2 = r_;
    p.next();
    a.add(t2, c.lp1In);
    d = p.tap(o.lp1B + 1);
    a.add(d, c.lp1Fb);
    p.next();
    a.add(d, c.lp2In);
    x = p.tap(o.lp2B + 1);
    a.add(x, c.lp2Fb);
    p.store(o.lp1B, r_);
    p.next();
    a.add(d, c.loop);
    a.add(p.tap(o.feed), c.feed);
    p.store(o.lp2B, r_);
    a.add(x, c.lpMix);
    x = p.tap(o.tB1[0]);
    p.next();
    a.add(x, 32);
    d = p.tap(o.tB1[1]);
    a.add(d, c.tB1G);
    p.store(o.storeB, r_);
    p.next();
    a.add(d, c.tB1K);
    p.store(o.tB1[0], r_);
    a.add(x, -c.tB1G);
    p.allpass(o.tB2[0], o.tB2[1], o.tB1[1], o.tB2[0], c.tB2G, c.tB2K);
    p.allpass(o.tB3[0], o.tB3[1], o.tB2[1], o.tB3[0], c.tB3G, c.tB3K);
    p.allpass(o.tB4[0], o.tB4[1], o.tB3[1], o.tB4[0], c.tB4G, c.tB4K);
    p.allpass(o.tB5[0], o.tB5[1], o.tB4[1], o.tB5[0], c.tB5G, c.tB5K);
    t = p.tap(o.outC[0]);
    p.next();
    a.add(t, c.outC[0]);
    a.add(p.tap(o.outC[1]), c.outC[1]);
    a.add(p.tap(o.outC[2]), c.outC[2]);
    a.add(p.tap(o.outC[3]), c.outC[3]);
    p.store(o.tB5[1], r_);
    r_ = a.result();
    s.dac[2] = r_;

    s.mem.advance();
}

} // namespace tearwash

namespace tearwash {

XlRegs Chamber::factory() const {
    XlRegs r;
    const uint8_t p1[6] = { 0x78, 0x62, 0x31, 0xE0, 0x00, 0x00 };
    const uint8_t p3[6] = { 0x89, 0x89, 0x80, 0xD0, 0x59, 0x02 };
    for (int i = 0; i < 6; ++i) { r.page[0][i] = p1[i]; r.page[2][i] = p3[i]; }
    r.size = 0x01;          // no SIZE map in this program
    r.options = 0xC0;
    return r;
}

// Control laws of CHAMBER (03 §3) [R]. No DEFINITION, no pre-echo pages, no SIZE map.
void Chamber::applyControls(const XlRegs& r) {
    using namespace law;
    const uint8_t lf = r.at(1, 1) > 0xF9 ? 0xF9 : r.at(1, 1);
    const uint8_t mid = r.at(1, 2) > 0xF9 ? 0xF9 : r.at(1, 2);
    const uint8_t xo = r.at(1, 3) < 0x08 ? 0x08 : r.at(1, 3);
    const int sMid = step5(mid);

    pair(r.at(3, 4), c.lpIn, c.lpFb);
    pair(r.at(1, 4), c.lp1In, c.lp1Fb);
    pair(xo, c.lp2In, c.lp2Fb);
    c.loop = sMid;
    c.lpMix = step5(lf) - sMid;
    g11_ = scaledGain(5, 11, sMid, 1);
    g10_ = scaledGain(5, 10, sMid, 1);
    setDecayReduction(reduction_);

    const int x = r.at(3, 5) >> 2;
    c.ap1G = c.ap2G = scaledGain(6, 26, x, 2);
    c.ap1K = c.ap2K = allpassK(c.ap1G);

    static const int kD0[4] = { 30, 30, 30, 5 }, kD1[4] = { 15, 20, 25, 2 }, kD2[4] = { 5, 10, 17, 30 },
                     kD3[4] = { 3, 8, 13, 20 };
    const uint8_t dep = r.at(1, 5);
    c.outA[0] = c.outC[0] = curve4(kD0, dep);
    c.outA[1] = c.outC[1] = -curve4(kD1, dep);
    c.outA[2] = c.outC[2] = curve4(kD2, dep);
    c.outA[3] = c.outC[3] = -curve4(kD3, dep);

    // PREDELAY: 34 samples per step, register at most E0.
    const uint8_t pd = r.at(1, 6) > 0xE0 ? 0xE0 : r.at(1, 6);
    o.monoTap = static_cast<uint16_t>(20973 + 34 * pd);
}

} // namespace tearwash
