// DSP verification (docs/spec/07_VALIDATION.md §1). Exit code 0 = all pass.

#include "core/SynthEngine.hpp"
#include "presets/Presets.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace tannhauser;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...) do { if (cond) { ++g_pass; } else { ++g_fail; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static void setAll(SynthEngine& e, const ParamValues& v) { for (uint32_t i = 0; i < PARAM_COUNT; ++i) e.setParam(i, v[i]); }

// A clean single-line test patch: line I sine only, line II silent, no drift, no effects.
static ParamValues sinePatch() {
    ParamValues v = defaultValues();
    v[P_DRIFT] = 0.0;
    v[lineParam(0, LP_VCF_LEVEL)] = 0.0;
    v[lineParam(0, LP_SINE)] = 1.0;
    v[lineParam(0, LP_VEG_A)] = 0.0;
    v[lineParam(0, LP_VEG_S)] = 1.0;
    v[lineParam(1, LP_LEVEL)] = 0.0;
    v[P_MIX] = 0.0;
    return v;
}

static std::vector<float> render(SynthEngine& e, double sec, int sr) {
    std::vector<float> l(static_cast<size_t>(sec * sr)), r(l.size());
    e.process(l.data(), r.data(), static_cast<int>(l.size()));
    return l;
}

// f0 from rising zero crossings with linear interpolation (skips the first 0.1 s).
static double measureF0(const std::vector<float>& x, int sr) {
    double first = -1, last = -1;
    int count = 0;
    for (size_t i = static_cast<size_t>(0.1 * sr) + 1; i < x.size(); ++i) {
        if (x[i - 1] < 0.f && x[i] >= 0.f) {
            const double t = (i - 1) + x[i - 1] / (x[i - 1] - x[i]);
            if (first < 0) first = t; else ++count;
            last = t;
        }
    }
    return count > 0 ? count * sr / (last - first) : 0.0;
}

static double cents(double a, double b) { return 1200.0 * std::log2(a / b); }

static double rms(const std::vector<float>& x, size_t a, size_t b) {
    double s = 0.0;
    b = std::min(b, x.size());
    for (size_t i = a; i < b; ++i) s += double(x[i]) * x[i];
    return b > a ? std::sqrt(s / double(b - a)) : 0.0;
}

static void testPitch(int sr) {
    const double feet[6] = { 0.5, 1, 1.5, 2, 3, 4 };
    for (int n : { 36, 48, 60, 72, 84 }) {
        for (int f = 0; f < 6; ++f) {
            if (f != 1 && n != 60) continue;   // footage only at middle C
            SynthEngine e;
            e.setSampleRate(sr);
            ParamValues v = sinePatch();
            v[lineParam(0, LP_FEET)] = f;
            setAll(e, v);
            e.noteOn(n, 1.0);
            const auto x = render(e, 1.0, sr);
            const double want = 261.6255653 * std::pow(2.0, (n - 60) / 12.0) * feet[f];
            const double got = measureF0(x, sr);
            CHECK(std::fabs(cents(got, want)) < 1.0, "T1/T3 pitch n=%d feet=%d sr=%d: %.3f Hz want %.3f", n, f, sr, got, want);
        }
    }
}

static void testLinearLaw() {
    // T2: with drift on, the offset error is roughly constant in Hz, so it is
    // larger in cents in the bass.
    const int sr = 48000;
    double errLowHz = 0, errHighHz = 0, errLowC = 0, errHighC = 0;
    for (int pass = 0; pass < 2; ++pass) {
        const int n = pass == 0 ? 36 : 84;
        SynthEngine e;
        e.setSampleRate(sr);
        ParamValues v = sinePatch();
        v[P_DRIFT] = 1.0;
        setAll(e, v);
        e.noteOn(n, 1.0);
        const auto x = render(e, 1.0, sr);
        const double want = 261.6255653 * std::pow(2.0, (n - 60) / 12.0);
        const double got = measureF0(x, sr);
        (pass == 0 ? errLowHz : errHighHz) = got - want;
        (pass == 0 ? errLowC : errHighC) = cents(got, want);
    }
    std::printf("T2 linear law: error C1 %.3f Hz (%.2f c), C5 %.3f Hz (%.2f c)\n", errLowHz, errLowC, errHighHz, errHighC);
    CHECK(std::fabs(errLowC) > std::fabs(errHighC), "T2 bass detune in cents should exceed treble");
}

static void testFilterEnvelope() {
    // T6: IL only -> starts and ends at -5*il; AL only -> peak +5*al, rests 0.
    const int sr = 48000;
    for (int mode = 0; mode < 2; ++mode) {
        SynthEngine e;
        e.setSampleRate(sr);
        ParamValues v = defaultValues();
        v[P_DRIFT] = 0.0;
        v[lineParam(0, LP_IL)] = mode == 0 ? 0.8 : 0.0;
        v[lineParam(0, LP_AL)] = mode == 0 ? 0.0 : 0.6;
        v[lineParam(0, LP_FEG_A)] = 0.3;
        v[lineParam(0, LP_FEG_D)] = 0.3;
        v[lineParam(0, LP_FEG_R)] = 0.2;
        v[lineParam(0, LP_INIT_BRILL)] = 0.0;
        v[lineParam(0, LP_AFTER_BRILL)] = 0.0;
        setAll(e, v);
        e.noteOn(60, 1.0);
        std::vector<float> l(16), r(16);
        e.process(l.data(), r.data(), 1);
        const double start = e.lastVfL(0, 0) - 10.0 * v[lineParam(0, LP_LPF)];
        double peak = -100;
        for (int i = 0; i < sr / 16; ++i) {
            e.process(l.data(), r.data(), 16);
            peak = std::max(peak, e.lastVfL(0, 0) - 7.0);
        }
        for (int i = 0; i < sr / 16; ++i) e.process(l.data(), r.data(), 16);
        const double rest = e.lastVfL(0, 0) - 7.0;
        e.noteOff(60);
        for (int i = 0; i < sr / 16; ++i) e.process(l.data(), r.data(), 16);
        const double after = e.lastVfL(0, 0) - 7.0;
        if (mode == 0) {
            CHECK(std::fabs(start + 4.0) < 0.2, "T6 IL start %.3f V want -4", start);
            CHECK(std::fabs(rest) < 0.05, "T6 IL rest %.3f V want 0", rest);
            CHECK(std::fabs(after + 4.0) < 0.1, "T6 IL release end %.3f V want -4", after);
        } else {
            CHECK(std::fabs(start) < 0.2, "T6 AL start %.3f V want 0", start);
            CHECK(std::fabs(peak - 3.0) < 0.1, "T6 AL peak %.3f V want 3", peak);
            CHECK(std::fabs(rest) < 0.05, "T6 AL rest %.3f V want 0", rest);
        }
    }
    // T5: the HPF sees kHP = 0.47 of the LPF's volts (slider and modulation).
    SynthEngine e;
    e.setSampleRate(sr);
    ParamValues v = defaultValues();
    v[lineParam(0, LP_AL)] = 1.0;
    v[lineParam(0, LP_HPF)] = 0.2;
    setAll(e, v);
    e.noteOn(60, 0.0);
    std::vector<float> l(64), r(64);
    for (int i = 0; i < 40; ++i) e.process(l.data(), r.data(), 64);
    const double dL = e.lastVfL(0, 0) - 7.0, dH = e.lastVfH(0, 0) - 0.47 * 2.0;
    CHECK(std::fabs(dH / dL - 0.47) < 0.01, "T5 HPF/LPF modulation ratio %.3f want 0.47", dH / dL);
}

static void testEnvelopeTimes() {
    // T7: attack slider 0 -> ~1 ms, 1 -> ~1 s (time to reach the peak).
    const int sr = 48000;
    for (double pos : { 0.0, 0.5, 1.0 }) {
        SynthEngine e;
        e.setSampleRate(sr);
        ParamValues v = sinePatch();
        v[lineParam(0, LP_VEG_A)] = pos;
        setAll(e, v);
        e.noteOn(60, 1.0);
        std::vector<float> l(1), r(1);
        int n = 0;
        while (e.voice(0).line[0].veg.stage() == AmpEnvelope::Attack && n < 3 * sr) { e.process(l.data(), r.data(), 1); ++n; }
        const double t = double(n) / sr, want = attackTimeSec(pos);
        CHECK(std::fabs(t / want - 1.0) < 0.15 || std::fabs(t - want) < 0.0006, "T7 attack pos %.1f: %.4f s want %.4f", pos, t, want);
    }
    // Release: time to fall to 10 %.
    for (double pos : { 0.0, 0.5 }) {
        SynthEngine e;
        e.setSampleRate(sr);
        ParamValues v = sinePatch();
        v[lineParam(0, LP_VEG_R)] = pos;
        setAll(e, v);
        e.noteOn(60, 1.0);
        std::vector<float> l(256), r(256);
        for (int i = 0; i < 40; ++i) e.process(l.data(), r.data(), 256);
        e.noteOff(60);
        int n = 0;
        while (e.voice(0).line[0].veg.value() > 0.1 && n < 20 * sr) { e.process(l.data(), r.data(), 1); ++n; }
        const double t = double(n) / sr, want = decayTimeSec(pos);
        CHECK(std::fabs(t / want - 1.0) < 0.15, "T7 release pos %.1f: %.4f s want %.4f", pos, t, want);
    }
}

static void testTouchPerVoice() {
    // T8: pressure on one key changes only that voice's brilliance.
    const int sr = 48000;
    SynthEngine e;
    e.setSampleRate(sr);
    ParamValues v = defaultValues();
    v[lineParam(0, LP_AFTER_BRILL)] = 1.0;
    setAll(e, v);
    e.noteOn(60, 0.5);
    e.noteOn(64, 0.5);
    e.noteOn(67, 0.5);
    std::vector<float> l(256), r(256);
    for (int i = 0; i < 20; ++i) e.process(l.data(), r.data(), 256);
    double before[3];
    for (int i = 0; i < 3; ++i) before[i] = e.lastVfL(i, 0);
    e.notePressure(64, 1.0);
    for (int i = 0; i < 200; ++i) e.process(l.data(), r.data(), 256);
    int changed = 0, which = -1;
    for (int i = 0; i < 3; ++i) if (std::fabs(e.lastVfL(i, 0) - before[i]) > 1.0) { ++changed; which = i; }
    CHECK(changed == 1 && e.voice(which).key == 64, "T8 pressure changed %d voices", changed);
}

static void testRingModEnvelope() {
    // T9: legato playing does not retrigger the ring-mod envelope.
    const int sr = 48000;
    SynthEngine e;
    e.setSampleRate(sr);
    ParamValues v = defaultValues();
    v[P_RM_ATTACK] = 0.0;
    v[P_RM_DECAY] = 0.6;
    setAll(e, v);
    std::vector<float> l(256), r(256);
    e.noteOn(60, 1);
    for (int i = 0; i < 100; ++i) e.process(l.data(), r.data(), 256);
    const double decayed = e.ringModEnvelope();
    e.noteOn(62, 1);   // legato: 60 still held
    for (int i = 0; i < 4; ++i) e.process(l.data(), r.data(), 256);
    CHECK(e.ringModEnvelope() <= decayed + 1e-6, "T9 legato retriggered the ring-mod envelope");
    e.noteOff(60); e.noteOff(62);
    for (int i = 0; i < 4; ++i) e.process(l.data(), r.data(), 256);
    e.noteOn(64, 1);
    e.process(l.data(), r.data(), 128);
    CHECK(e.ringModEnvelope() > 0.9, "T9 detached note did not retrigger (%.3f)", e.ringModEnvelope());
}

static void testSustainII() {
    // T10: a new note silences fading notes within 10 ms in Sustain II.
    const int sr = 48000;
    SynthEngine e;
    e.setSampleRate(sr);
    ParamValues v = sinePatch();
    v[lineParam(0, LP_VEG_R)] = 0.8;   // 2.5 s release
    v[P_SUS_MODE] = 1.0;
    setAll(e, v);
    std::vector<float> l(240), r(240);
    e.noteOn(60, 1);
    for (int i = 0; i < 20; ++i) e.process(l.data(), r.data(), 240);
    e.noteOff(60);
    for (int i = 0; i < 4; ++i) e.process(l.data(), r.data(), 240);
    e.noteOn(67, 1);
    for (int i = 0; i < 2; ++i) e.process(l.data(), r.data(), 240);   // 10 ms
    double level60 = 1.0;
    for (int i = 0; i < kNumVoices; ++i) if (e.voice(i).key == 60) level60 = e.voice(i).line[0].veg.value();
    CHECK(level60 < 0.2, "T10 Sustain II fading note still at %.3f", level60);
}

static void testGlide() {
    // T11: with portamento a new voice starts at the last played key.
    const int sr = 48000;
    SynthEngine e;
    e.setSampleRate(sr);
    ParamValues v = sinePatch();
    v[P_PORTA_TIME] = 0.4;
    setAll(e, v);
    std::vector<float> l(64), r(64);
    e.noteOn(48, 1);
    for (int i = 0; i < 10; ++i) e.process(l.data(), r.data(), 64);
    e.noteOn(60, 1);
    e.process(l.data(), r.data(), 1);
    int vi = -1;
    for (int i = 0; i < kNumVoices; ++i) if (e.voice(i).key == 60) vi = i;
    CHECK(vi >= 0 && std::fabs(e.voice(vi).smoothSemis - 48.0) < 0.5, "T11 glide start %.2f want 48", vi >= 0 ? e.voice(vi).smoothSemis : -1.0);
    for (int i = 0; i < 2000; ++i) e.process(l.data(), r.data(), 64);
    CHECK(vi >= 0 && std::fabs(e.voice(vi).smoothSemis - 60.0) < 0.01, "T11 glide end %.2f want 60", e.voice(vi).smoothSemis);
}

static void testFilterNoSelfOscillation() {
    // T4: max resonance, an impulse-like start then silence must decay.
    const int sr = 48000;
    for (double lpf : { 0.1, 0.5, 1.0 }) {
        SynthEngine e;
        e.setSampleRate(sr);
        ParamValues v = defaultValues();
        v[lineParam(0, LP_RES_L)] = 1.0;
        v[lineParam(0, LP_RES_H)] = 1.0;
        v[P_RESONANCE] = 1.0;
        v[lineParam(0, LP_LPF)] = lpf;
        v[lineParam(0, LP_SAW)] = 0.0;
        v[lineParam(0, LP_NOISE)] = 1.0;
        v[lineParam(0, LP_AL)] = 0.0;
        v[lineParam(0, LP_VEG_S)] = 1.0;
        v[lineParam(1, LP_LEVEL)] = 0.0;
        setAll(e, v);
        e.noteOn(60, 1.0);
        auto x = render(e, 0.3, sr);
        e.setParam(lineParam(0, LP_NOISE), 0.0);
        auto y = render(e, 0.6, sr);
        const double tail = rms(y, y.size() - sr / 10, y.size());
        CHECK(tail < 1e-3, "T4 lpf %.1f: filter rings on after the input stopped (%.5f)", lpf, tail);
    }
}

static void testPresetsRobust() {
    // T12: every preset, 8-note chord: finite output, peak < 1.
    PresetLibrary lib;
    const int sr = 48000;
    for (const auto& p : lib.presets()) {
        SynthEngine e;
        e.setSampleRate(sr);
        setAll(e, p.values);
        for (int k : { 36, 43, 48, 55, 60, 64, 67, 72 }) e.noteOn(k, 1.0);
        for (int k : { 36, 43, 48, 55, 60, 64, 67, 72 }) e.notePressure(k, 1.0);
        std::vector<float> l(48000), r(48000);
        float peak = 0.f;
        bool finite = true;
        for (int b = 0; b < 2; ++b) {
            e.process(l.data(), r.data(), 48000);
            for (size_t i = 0; i < l.size(); ++i) {
                if (!std::isfinite(l[i]) || !std::isfinite(r[i])) finite = false;
                peak = std::max(peak, std::max(std::fabs(l[i]), std::fabs(r[i])));
            }
            for (int k : { 36, 43, 48, 55, 60, 64, 67, 72 }) e.noteOff(k);
        }
        CHECK(finite && peak < 1.0f, "T12 preset '%s' finite=%d peak=%.3f", p.name.c_str(), finite, peak);
    }
}

static void testSampleRateInvariance() {
    // T13: RMS of a filtered chord at 44.1/48/96 kHz within 0.5 dB.
    double ref = 0.0;
    for (int sr : { 48000, 44100, 96000 }) {
        SynthEngine e;
        e.setSampleRate(sr);
        ParamValues v = defaultValues();
        v[P_DRIFT] = 0.0;
        setAll(e, v);
        e.noteOn(48, 0.8); e.noteOn(60, 0.8);
        auto x = render(e, 1.0, sr);
        const double r = rms(x, x.size() / 2, x.size());
        if (sr == 48000) ref = r;
        else CHECK(std::fabs(20.0 * std::log10(r / ref)) < 0.5, "T13 sr %d rms %.4f vs %.4f", sr, r, ref);
    }
}

static void testCpu() {
    // T15: 8 voices (16 lines), 48 kHz.
    const int sr = 48000;
    SynthEngine e;
    e.setSampleRate(sr);
    ParamValues v = defaultValues();
    v[lineParam(0, LP_SQUARE)] = 1.0;
    v[lineParam(1, LP_SQUARE)] = 1.0;
    v[P_CHORUS] = 1.0;
    v[P_REV_MIX] = 0.3;
    v[P_SUB_VCO] = 0.2;
    setAll(e, v);
    for (int k : { 36, 43, 48, 55, 60, 64, 67, 72 }) e.noteOn(k, 0.8);
    std::vector<float> l(256), r(256);
    const int blocks = 5 * sr / 256;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < blocks; ++i) e.process(l.data(), r.data(), 256);
    const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const double rtf = sec / 5.0;
    std::printf("T15 CPU: real-time factor %.3f for 16 lines at 48 kHz\n", rtf);
    CHECK(rtf < 0.5, "T15 real-time factor %.3f", rtf);
}

int main() {
    testPitch(48000);
    testPitch(44100);
    testLinearLaw();
    testFilterEnvelope();
    testEnvelopeTimes();
    testTouchPerVoice();
    testRingModEnvelope();
    testSustainII();
    testGlide();
    testFilterNoSelfOscillation();
    testPresetsRobust();
    testSampleRateInvariance();
    testCpu();
    std::printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
