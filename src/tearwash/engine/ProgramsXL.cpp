// 224XL algorithm networks (docs/tearwash/03 §1). Statement order is evaluation order: every
// tap is read and every store made at the point the original program does, so delay timing
// within a sample, rounding and saturation match it [R]. Comments name the sections.

#include "Programs.hpp"

namespace tearwash {

void ConcertHall::tick(CoreState& s) {
    Pipe p{ s, a_, r_ };
    Mac& a = a_;
    int16_t x, d, t;

    // ---- Left half --------------------------------------------------------------------------
    // Input bandwidth one-pole: lp = A·in + B·lp[-1]; the previous sample's last sum is stored.
    p.next();
    a.add(s.inL, c.inA);
    p.store(o.carry, r_);
    a.add(p.tap(o.lpL + 1), c.inB);
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
    // Treble one-pole, output B.
    x = p.tap(o.xoInL);
    p.next();
    a.add(x, c.trebleA);
    a.add(p.tap(o.xoL + 1), c.trebleB);
    s.dac[1] = r_;
    p.next();
    a.add(x, c.fb);
    p.store(o.xoL, r_);
    a.add(r_, c.xo);
    a.add(p.tap(o.fbL), c.fbTap);
    // Mid-decay one-pole (continues into the first decay allpass without clearing).
    t = p.tap(o.midTapL);
    p.next();
    a.add(t, c.midA);
    a.add(r_, c.midB);
    d = p.tap(o.dec1L[1]);
    r_ = a.result();
    a.add(d, c.dec1G);
    p.store(o.midL, r_);
    x = r_;
    p.next();
    a.add(d, c.dec1K);
    p.store(o.dec1L[0], r_);
    a.add(x, -c.dec1G);
    p.allpass(o.dec2L[0], o.dec2L[1], o.dec1L[1], o.dec2L[0], c.dec2G, c.dec2K);
    // Output A taps.
    t = p.tap(o.outA[0]);
    p.next();
    a.add(t, c.outA[0]);
    p.store(o.decOutL, r_);
    a.add(r_, c.outA[1]);
    a.add(p.tap(o.outA[1]), c.outA[2]);
    a.add(p.tap(o.outA[2]), c.outA[3]);
    t = p.tap(o.xferA);
    p.next();
    a.add(t, 32);
    s.dac[0] = r_;
    r_ = a.result();
    p.store(o.xferAOut, r_);

    // ---- Right half -------------------------------------------------------------------------
    p.next();
    a.add(s.inR, c.inA);
    a.add(p.tap(o.lpR + 1), c.inB);
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
    a.add(x, -c.trebleA);
    a.add(p.tap(o.xoR + 1), c.trebleB);
    s.dac[3] = r_;
    p.next();
    a.add(x, -c.fb);
    p.store(o.xoR, r_);
    a.add(r_, c.xo);
    a.add(p.tap(o.fbR), c.fbTap);
    t = p.tap(o.midTapR);
    p.next();
    a.add(t, c.midA);
    a.add(r_, c.midB);
    d = p.tap(o.dec1R[1]);
    r_ = a.result();
    a.add(d, c.dec1G);
    p.store(o.midR, r_);
    x = r_;
    p.next();
    a.add(d, c.dec1K);
    p.store(o.dec1R[0], r_);
    a.add(x, -c.dec1G);
    p.allpass(o.dec2R[0], o.dec2R[1], o.dec1R[1], o.dec2R[0], c.dec2G, c.dec2K);
    // Output C taps.
    t = p.tap(o.outC[0]);
    p.next();
    a.add(t, c.outC[0]);
    a.add(p.tap(o.outC[1]), c.outC[1]);
    a.add(p.tap(o.outC[2]), c.outC[2]);
    a.add(p.tap(o.outC[3]), c.outC[3]);
    p.store(o.decOutR, r_);
    t = p.tap(o.xferC);
    p.next();
    a.add(t, 32);
    s.dac[2] = r_;

    s.mem.advance();
}

} // namespace tearwash
