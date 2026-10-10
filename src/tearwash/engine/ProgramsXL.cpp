// 224XL algorithm networks (docs/tearwash/03 §1). Statement order is evaluation order: every
// tap is read and every store made at the point the original program does, so delay timing
// within a sample, rounding and saturation match it [R]. Comments name the sections.

#include "Programs.hpp"

#include <algorithm>

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

XlRegs ConcertHall::factory() const {
    XlRegs r;
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

    // PREDELAY (millisecond law, 34 samples per ms, limited by the memory map).
    int pd = 34 * predelayMs(r.at(1, 6));
    if (pd > 22780) pd = 22780;
    o.preL = static_cast<uint16_t>(19680 + pd);
    o.preR = static_cast<uint16_t>(42608 + pd);

    // Pre-echoes: LEVELs, DELAYs (34 samples per step) with FINE; the decay taps (4 per step).
    for (int i = 0; i < 4; ++i) c.echo[i] = r.at(4, i + 1) >> 2;
    o.echo[0] = static_cast<uint16_t>(19680 + fineDelay(r.at(5, 1), r.at(6, 1), 34));
    o.echo[1] = static_cast<uint16_t>(42608 + fineDelay(r.at(5, 2), r.at(6, 2), 34));
    o.echo[2] = static_cast<uint16_t>(42608 + fineDelay(r.at(5, 3), r.at(6, 3), 34));
    o.echo[3] = static_cast<uint16_t>(19680 + fineDelay(r.at(5, 4), r.at(6, 4), 34));
    o.decOutL = static_cast<uint16_t>(1448 + fineDelay(r.at(5, 5), r.at(6, 5), 4));
    o.outC[1] = static_cast<uint16_t>(11344 + fineDelay(r.at(5, 6), r.at(6, 6), 4));
}

} // namespace tearwash
