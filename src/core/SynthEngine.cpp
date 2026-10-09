#include "SynthEngine.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace tannhauser {

namespace {

constexpr double kMiddleC = 261.6255653005986;   // MIDI 60 at 8' (Yamaha "C3"), spec 02 §1
constexpr double kKv025Hz = 130.8127826502993;   // KV 0.25 V = C2 at 8' (IG00153 table)
constexpr double kFeetRatio[6] = { 0.5, 1.0, 1.5, 2.0, 3.0, 4.0 };
constexpr double kHpfWeight = 0.47;              // HPF slider: Vfc via 100k vs LPF via 47k (M board), spec 02 §4
constexpr double kHpfOctaveRatio = 0.5;          // HPF moves half the LPF's octaves (Cherry, measured), P-4
constexpr double kBusGain = 0.15;

inline double semisToHz(double semis) { return kMiddleC * std::exp2((semis - 60.0) / 12.0); }

// Soft OTA-input saturation ~ 1.2 tanh(x / 1.2), rational approximation.
inline double softSat(double x) {
    const double y = clampd(x * (1.0 / 1.2), -3.0, 3.0);
    const double y2 = y * y;
    return 1.2 * y * (27.0 + y2) / (27.0 + 9.0 * y2);
}

// SVF prewarp tan(pi fc / fs) for fc <= 0.45 fs (x <= 1.414): Pade [5/4],
// relative error < 1e-5 over that range.
inline double prewarp(double fc, double fs) {
    const double x = kPi * fc / fs;
    const double x2 = x * x;
    return x * (945.0 - 105.0 * x2 + x2 * x2) / (945.0 - 420.0 * x2 + 15.0 * x2 * x2);
}

} // namespace

SynthEngine::SynthEngine() {
    for (uint32_t i = 0; i < PARAM_COUNT; ++i) params_[i] = paramInfo(i).def;
    smooth_ = params_;
    initCalibration();
    setSampleRate(48000.0);
}

void SynthEngine::setCalibrationSeed(uint64_t seed) {
    calibrationSeed_ = seed;
    initCalibration();
}

void SynthEngine::initCalibration() {
    for (int v = 0; v < kNumVoices; ++v) {
        for (int l = 0; l < 2; ++l) {
            Rng r(calibrationSeed_ * 0x9E3779B1ull + static_cast<uint64_t>(v * 2 + l) * 7919ull + 1);
            for (int k = 0; k < 4; ++k) r.next();
            CardCalibration& c = voices_[v].line[l].cal;
            c.scaleErr = 0.0015 * r.bipolar();
            c.offsetHz = 0.6 * r.bipolar();
            c.pulseWidth = 1.0 + 0.3 * r.bipolar();
            c.pulseHeight = 1.0 + 0.3 * r.bipolar();
            c.cutScale = 0.04 * r.bipolar();
            c.qScale = 0.05 * r.bipolar();
            c.envScale = 0.06 * r.bipolar();
            c.feedthrough = std::pow(10.0, (-70.0 + 10.0 * r.uniform()) / 20.0);
            c.phase0 = r.uniform();
            voices_[v].line[l].phase = c.phase0;
        }
    }
}

void SynthEngine::setSampleRate(double fs) {
    if (!(fs > 1000.0) || !std::isfinite(fs)) return;
    fs_ = fs;
    fsOs_ = fs * kOversample;
    glideCoeff_ = 1.0 - std::exp(-1.0 / (0.005 * fsOs_));
    scoopDecay_ = std::exp(-1.0 / (0.06 * fsOs_));
    chorus_.setSampleRate(fs);
    reverb_.setSampleRate(fs);
    for (auto& v : voices_) for (auto& l : v.line) l.pole.setCutoff(7600.0, fsOs_);
    mono_.assign(kSubBlock, 0.0f);
    wetL_.assign(kSubBlock, 0.0f);
    wetR_.assign(kSubBlock, 0.0f);
    reset();
}

void SynthEngine::reset() {
    for (auto& v : voices_) {
        v.key = -1; v.gate = false; v.fastRelease = false;
        v.pressure = v.pressureTarget = 0.0;
        v.scoop = 0.0;
        for (auto& l : v.line) {
            l.hp.reset(); l.lp.reset(); l.pole.reset();
            l.feg.reset(); l.veg.reset();
            l.drift.x = 0.0;
            l.noiseLevel = 0.0;
        }
    }
    heldKeys_ = 0;
    rmEnv_ = 0.0; rmAttack_ = false;
    decimator_.reset();
    chorus_.reset();
    reverb_.reset();
    smoothInit_ = false;
}

void SynthEngine::setParam(uint32_t id, double value) {
    if (id >= PARAM_COUNT) return;
    params_[id] = clampParam(id, value);
}

// --- Key assigner (spec 03 §1) ---------------------------------------------------

int SynthEngine::allocateVoice(int key) {
    // 1. Re-struck key keeps its voice.
    for (int i = 0; i < kNumVoices; ++i) {
        if (voices_[i].key == key && (voices_[i].gate || voices_[i].active())) return i;
    }
    // 2. First free voice in round-robin order.
    for (int k = 0; k < kNumVoices; ++k) {
        const int i = (nextVoice_ + k) % kNumVoices;
        if (!voices_[i].gate && !voices_[i].active()) {
            nextVoice_ = (i + 1) % kNumVoices;
            return i;
        }
    }
    // 3. The voice released longest ago.
    int best = -1;
    for (int i = 0; i < kNumVoices; ++i) {
        if (!voices_[i].gate && (best < 0 || voices_[i].offOrder < voices_[best].offOrder)) best = i;
    }
    if (best >= 0) return best;
    // 4. The oldest held note.
    best = 0;
    for (int i = 1; i < kNumVoices; ++i) {
        if (voices_[i].onOrder < voices_[best].onOrder) best = i;
    }
    return best;
}

double SynthEngine::portaPeriodSec() const {
    const double x = params_[P_PORTA_TIME];
    return x <= 0.0 ? 0.0 : 0.0014 * std::pow(1000.0, x);
}

void SynthEngine::startVoice(Voice& v, int key, double velocity) {
    v.key = key;
    v.gate = true;
    v.fastRelease = false;
    v.velocity = velocity;
    v.onOrder = ++noteCounter_;
    v.keySemis = key;
    if (portaPeriodSec() > 0.0 && havePlayed_) {
        // Polyphonic glide: every new voice starts from the last played key.
        v.rawSemis = lastPlayedSemis_;
        v.glissStart = lastPlayedSemis_;
        v.glissClock = 0.0;
        v.smoothSemis = lastPlayedSemis_;
    } else {
        v.rawSemis = v.smoothSemis = v.glissStart = key;
    }
    lastPlayedSemis_ = key;
    havePlayed_ = true;
    v.scoop = -2.0 * params_[P_TOUCH_BEND] * velocity;
    for (int l = 0; l < 2; ++l) {
        const uint32_t base = l == 0 ? kLine1Base : kLine2Base;
        v.line[l].feg.gateOn(params_[base + LP_IL], params_[base + LP_AL]);
        v.line[l].veg.gateOn();
    }
}

void SynthEngine::noteOn(int key, double velocity) {
    if (key < 0 || key > 127) return;
    velocity = clampd(std::isfinite(velocity) ? velocity : 1.0, 0.0, 1.0);
    int held = 0;
    for (const auto& v : voices_) held += v.gate ? 1 : 0;
    // Ring modulator envelope: monophonic, only after all keys were up (spec 03 §9).
    if (held == 0) rmAttack_ = true;
    // Sustain II: a new note silences notes that are still fading.
    if (params_[P_SUS_MODE] >= 0.5) {
        for (auto& v : voices_) if (!v.gate && v.active()) v.fastRelease = true;
    }
    const int i = allocateVoice(key);
    startVoice(voices_[i], key, velocity);
}

void SynthEngine::noteOff(int key) {
    for (auto& v : voices_) {
        if (v.gate && v.key == key) {
            v.gate = false;
            v.offOrder = ++noteCounter_;
            for (auto& l : v.line) { l.feg.gateOff(); l.veg.gateOff(); }
        }
    }
}

void SynthEngine::notePressure(int key, double pressure) {
    if (!std::isfinite(pressure)) return;
    pressure = clampd(pressure, 0.0, 1.0);
    for (auto& v : voices_) if (v.gate && v.key == key) v.pressureTarget = pressure;
}

void SynthEngine::channelPressure(double pressure) {
    if (!std::isfinite(pressure)) return;
    pressure = clampd(pressure, 0.0, 1.0);
    for (auto& v : voices_) if (v.gate) v.pressureTarget = pressure;
}

void SynthEngine::pitchBend(double norm) {
    if (std::isfinite(norm)) bendNorm_ = clampd(norm, -1.0, 1.0);
}

void SynthEngine::modWheel(double v) {
    if (std::isfinite(v)) modWheel_ = clampd(v, 0.0, 1.0);
}

void SynthEngine::allNotesOff() {
    for (auto& v : voices_) if (v.gate) noteOff(v.key);
}

void SynthEngine::allSoundOff() {
    for (auto& v : voices_) {
        v.gate = false;
        v.key = -1;
        for (auto& l : v.line) { l.veg.reset(); l.feg.reset(); l.hp.reset(); l.lp.reset(); l.pole.reset(); }
    }
}

int SynthEngine::activeVoiceCount() const {
    int n = 0;
    for (const auto& v : voices_) n += v.active() ? 1 : 0;
    return n;
}

double SynthEngine::lineFrequency(int vi, int l) const {
    const Voice& v = voices_[vi];
    const LineControls& c = lc_[l];
    const CardCalibration& cal = v.line[l].cal;
    const double drift = params_[P_DRIFT];
    const double semis = v.smoothSemis + v.scoop + params_[P_PITCH] + ribbonSm_ + bendNorm_ * params_[P_BEND_RANGE];
    const double detune = (l == 1) ? 12.0 * params_[P_DETUNE] * params_[P_DETUNE] : 0.0;
    return (semisToHz(semis) + detune) * c.feetRatio * (1.0 + cal.scaleErr * drift)
           + cal.offsetHz * drift + v.line[l].drift.x * drift;
}

// --- Control-rate update -------------------------------------------------------------

void SynthEngine::updateControls(int hostSamples) {
    const double blockSec = hostSamples / fs_;
    if (!smoothInit_) {
        smooth_ = params_;
        smoothInit_ = true;
    } else {
        const double c = 1.0 - std::exp(-blockSec / 0.008);
        for (uint32_t i = 0; i < PARAM_COUNT; ++i) {
            if (paramInfo(i).flags & PF_STEPPED && i != lineParam(0, LP_SAW) && i != lineParam(0, LP_SQUARE)
                && i != lineParam(1, LP_SAW) && i != lineParam(1, LP_SQUARE)) {
                smooth_[i] = params_[i];
            } else {
                smooth_[i] += (params_[i] - smooth_[i]) * c;
            }
        }
    }
    const double* p = smooth_.data();
    const double globalRes = p[P_RESONANCE];
    const double mix = p[P_MIX];
    for (int l = 0; l < 2; ++l) {
        const uint32_t b = l == 0 ? kLine1Base : kLine2Base;
        LineControls& c = lc_[l];
        c.feetRatio = kFeetRatio[static_cast<int>(clampd(params_[b + LP_FEET], 0, 5))];
        c.pwBase = 0.5 + 0.4 * p[b + LP_PW];
        c.pwmDepth = p[b + LP_PWM_DEPTH];
        c.sawOn = p[b + LP_SAW];
        c.squareOn = p[b + LP_SQUARE];
        c.noise = p[b + LP_NOISE];
        c.hpfV = 10.0 * p[b + LP_HPF];
        c.lpfV = 10.0 * p[b + LP_LPF];
        c.vqH = clampd(10.0 * (1.0 - p[b + LP_RES_H]) - 10.0 * globalRes, 0.0, 10.0);
        c.vqL = clampd(10.0 * (1.0 - p[b + LP_RES_L]) - 10.0 * globalRes, 0.0, 10.0);
        c.il = p[b + LP_IL];
        c.al = p[b + LP_AL];
        c.fegA = attackTimeSec(p[b + LP_FEG_A]);
        c.fegD = decayTimeSec(p[b + LP_FEG_D]);
        c.fegR = decayTimeSec(p[b + LP_FEG_R]);
        c.vcfLevel = p[b + LP_VCF_LEVEL];
        c.sine = p[b + LP_SINE];
        c.vegA = attackTimeSec(p[b + LP_VEG_A]);
        c.vegD = decayTimeSec(p[b + LP_VEG_D]);
        c.vegS = p[b + LP_VEG_S];
        c.vegR = decayTimeSec(p[b + LP_VEG_R]);
        c.level = p[b + LP_LEVEL];
        c.initBrill = p[b + LP_INIT_BRILL];
        c.initLevel = p[b + LP_INIT_LEVEL];
        c.afterBrill = p[b + LP_AFTER_BRILL];
        c.afterLevel = p[b + LP_AFTER_LEVEL];
        c.mixGain = l == 0 ? std::min(1.0, 2.0 * (1.0 - mix)) : std::min(1.0, 2.0 * mix);
        // PWM LFO, one per line (spec 03 §4).
        const double pwmHz = 0.1 * std::pow(250.0, p[b + LP_PWM_SPEED]);
        pwmPhase_[l] = wrap01(pwmPhase_[l] + pwmHz * blockSec);
        c.pwmLfo = std::sin(kTwoPi * pwmPhase_[l]);
    }

    // Per voice: touch smoothing, keyboard control, envelopes' coefficients via lc_.
    const double pressC = 1.0 - std::exp(-blockSec / 0.1);   // TWS A output, ~0.1 s
    const double drift = params_[P_DRIFT];
    for (auto& v : voices_) {
        v.pressure += (v.pressureTarget - v.pressure) * pressC;
        const double t = clampd((v.keySemis - 36.0) / 60.0, 0.0, 1.0);
        auto kbd = [t](double low, double high) { return t < 0.5 ? low * (1.0 - 2.0 * t) : high * (2.0 * t - 1.0); };
        v.kbdBrillV = 3.0 * kbd(p[P_KBD_BRILL_LOW], p[P_KBD_BRILL_HIGH]);
        v.kbdLevel = std::max(0.0, 1.0 + 0.8 * kbd(p[P_KBD_LEVEL_LOW], p[P_KBD_LEVEL_HIGH]));
        if (!v.active() && !v.gate) continue;
        for (int li = 0; li < 2; ++li) {
            LineState& s = v.line[li];
            const LineControls& c = lc_[li];
            s.drift.step(rng_, blockSec, 20.0, 0.25);
            // Envelope coefficients; release extended by the sustain pedal,
            // Sustain II's silenced notes fade in 5 ms (spec 03 §8).
            const double envK = 1.0 + s.cal.envScale * drift;
            double relV = c.vegR * envK, relF = c.fegR * envK;
            if (params_[P_SUS_PEDAL] >= 0.5) {
                const double ts = decayTimeSec(p[P_SUS_TIME]);
                relV += ts; relF += ts;
            }
            if (v.fastRelease) relV = relF = 0.005;
            s.fegCa = attackCoeff(c.fegA * envK, fsOs_);
            s.fegCd = decayCoeff(c.fegD * envK, fsOs_);
            s.fegCr = decayCoeff(relF, fsOs_);
            s.vegCa = attackCoeff(c.vegA * envK, fsOs_);
            s.vegCd = decayCoeff(c.vegD * envK, fsOs_);
            s.vegCr = decayCoeff(relV, fsOs_);
            const double qScale = 1.0 + s.cal.qScale * drift;
            s.qaH = 5.0 * std::pow(0.1, c.vqH * 0.1) * qScale;
            s.qaL = 5.0 * std::pow(0.1, c.vqL * 0.1) * qScale;
        }
    }
    // Ribbon: smoothed ~2 ms (spec 03 §7). The parameter is -1..1 = +-1 octave.
    ribbonSm_ += (12.0 * params_[P_RIBBON] - ribbonSm_) * (1.0 - std::exp(-blockSec / 0.002));
}

// --- Voice rendering -------------------------------------------------------------------

double SynthEngine::renderVoiceSample(Voice& v, int vi, double noise, double subLfo) {
    // Pitch: glide / glissando at the PC clock (spec 03 §2), then the low-pass.
    const double T = portaPeriodSec();
    if (T > 0.0 && v.rawSemis != v.keySemis) {
        const double step = 1.0 / (T * fsOs_);
        if (params_[P_PORTA_MODE] < 0.5) {
            if (v.rawSemis < v.keySemis) v.rawSemis = std::min(v.keySemis, v.rawSemis + step);
            else v.rawSemis = std::max(v.keySemis, v.rawSemis - step);
        } else {
            v.glissClock += step;
            if (v.glissClock >= 1.0) {
                v.glissClock -= 1.0;
                v.rawSemis += (v.keySemis > v.rawSemis) ? 1.0 : -1.0;
                if (std::fabs(v.rawSemis - v.keySemis) < 1.0) v.rawSemis = v.keySemis;
            }
        }
    } else {
        v.rawSemis = v.keySemis;
    }
    v.smoothSemis += (v.rawSemis - v.smoothSemis) * (T > 0.0 ? glideCoeff_ : 1.0);
    v.scoop *= scoopDecay_;

    const double* p = smooth_.data();
    const double drift = params_[P_DRIFT];
    if (updateFilters_) {
        // Key voltage and VCO frequency, once per host sample.
        const double kvSemis = v.smoothSemis + params_[P_PITCH] + ribbonSm_ + bendNorm_ * params_[P_BEND_RANGE];
        v.fKv = semisToHz(kvSemis);
        // VCO-only exponential modulators: sub-osc vibrato (+ touch depth, mod wheel) and scoop.
        const double vibDepth = clampd(p[P_SUB_VCO] + 0.5 * modWheel_ + p[P_TOUCH_VCO] * v.pressure, 0.0, 1.0);
        const double vcoSemis = v.scoop + 12.0 * vibDepth * vibDepth * subLfo;
        v.fVco = v.fKv * std::exp2(vcoSemis * (1.0 / 12.0));
    }
    const double fKv = v.fKv, fVco = v.fVco;

    const double vcfSubDepth = clampd(p[P_SUB_VCF] + p[P_TOUCH_VCF] * v.pressure, 0.0, 1.0);
    const double subVcfV = 5.0 * vcfSubDepth * subLfo;
    const double subVca = 1.0 - p[P_SUB_VCA] * (0.5 - 0.5 * subLfo);
    const double detuneHz = 12.0 * p[P_DETUNE] * p[P_DETUNE];

    double out = 0.0;
    for (int l = 0; l < 2; ++l) {
        LineState& s = v.line[l];
        const LineControls& c = lc_[l];
        const CardCalibration& cal = s.cal;
        s.feg.setLevels(c.il, c.al);
        const double fegV = s.feg.tick(s.fegCa, s.fegCd, s.fegCr);
        const double veg = s.veg.tick(s.vegCa, s.vegCd, c.vegS, s.vegCr);
        if (!s.veg.active()) { s.phase = wrap01(s.phase + fVco / fsOs_); continue; }

        // VCO: linear Hz law, offsets in Hz (spec 02 §1).
        double f = (fVco + (l == 1 ? detuneHz : 0.0)) * c.feetRatio * (1.0 + cal.scaleErr * drift)
                   + (cal.offsetHz + s.drift.x) * drift;
        f = clampd(f, 0.0, 0.45 * fsOs_);
        const double dt = f / fsOs_;
        s.phase += dt;
        if (s.phase >= 1.0) s.phase -= 1.0;
        const double t = s.phase;

        double osc = 0.0;
        if (c.sawOn > 1e-4) {
            double saw = 2.0 * t - 1.0 - polyBlep(t, dt);
            // Start pulse at each reset (spec 02 §2).
            const double w = clampd(0.02 * cal.pulseWidth, 2.5 * dt, 0.2);
            const double h = 0.2 * cal.pulseHeight;
            saw += (t < w ? h : 0.0) + 0.5 * h * polyBlep(t, dt) - 0.5 * h * polyBlep(t >= w ? t - w : t - w + 1.0, dt);
            osc += c.sawOn * saw;
        }
        if (c.squareOn > 1e-4) {
            const double pw = clampd(c.pwBase + 0.4 * c.pwmDepth * c.pwmLfo, 0.5, 0.95);
            double pulse = (t < pw ? 1.0 : -1.0) + polyBlep(t, dt) - polyBlep(t >= pw ? t - pw : t - pw + 1.0, dt);
            pulse -= 2.0 * pw - 1.0;
            osc += c.squareOn * pulse;
        }
        s.noiseLevel += (c.noise - s.noiseLevel) * 0.01;
        osc += 0.7 * s.noiseLevel * noise;

        double sine = 0.0;
        if (c.sine > 1e-4) {
            const double tri = t < 0.5 ? 4.0 * t - 1.0 : 3.0 - 4.0 * t;
            sine = 0.86 * (std::sin(0.5 * kPi * tri) + 0.03 * tri * tri * tri) / 1.03;
        }

        // Filters: Vf sums (spec 02 §4).
        const double touchBrillV = 4.0 * c.initBrill * v.velocity + 5.0 * c.afterBrill * v.pressure;
        const double mods = fegV + 4.0 * p[P_BRILLIANCE] + touchBrillV + v.kbdBrillV + subVcfV;
        const double vfL = clampd(c.lpfV + mods, 0.0, 20.0);
        const double track = fKv / kKv025Hz * 200.0 * (1.0 + cal.cutScale * drift);   // Hz per volt
        const double fcMax = 0.45 * fsOs_;
        const double fcL = clampd(vfL * track, 20.0, fcMax);
        // HPF: its own slider through the 0.47 divider, moved by half the
        // octaves the modulators move the LPF (spec 02 §4).
        const double ratio = (vfL + 1.0) / (c.lpfV + 1.0);
        const double hpOct = kHpfOctaveRatio == 0.5 ? std::sqrt(ratio) : std::pow(ratio, kHpfOctaveRatio);
        const double fcH = clampd(std::max(20.0, kHpfWeight * c.hpfV * track) * hpOct, 20.0, fcMax);
        lastVfL_[vi][l] = vfL;
        lastVfH_[vi][l] = fcH / track;
        if (updateFilters_) {
            // Q falls from QA toward 0.5 above fq (spec 02 §4).
            auto damping = [](double qa, double fc) {
                const double fq = 1100.0 * 14.4 / qa;
                const double r = fc / fq;
                const double q = qa / (1.0 + r) + 0.5 * r / (1.0 + r);
                return 1.0 / std::max(q, 0.3);
            };
            s.hp.setCoeffs(prewarp(fcH, fsOs_), damping(s.qaH, fcH));
            s.lp.setCoeffs(prewarp(fcL, fsOs_), damping(s.qaL, fcL));
        }

        double lp, bp, hp;
        s.hp.tick(softSat(osc), lp, bp, hp);
        const double hpOut = hp;
        s.lp.tick(hpOut, lp, bp, hp);
        const double filtered = s.pole.tick(lp);

        const double pre = c.vcfLevel * filtered + c.sine * sine;
        const double dyn = (1.0 - 0.75 * c.initLevel) + 0.75 * c.initLevel * v.velocity + 0.5 * c.afterLevel * v.pressure;
        const double gain = veg * dyn * c.level * v.kbdLevel * subVca * c.mixGain;
        out += pre * (gain + cal.feedthrough * drift);
    }
    return out;
}

void SynthEngine::process(float* outL, float* outR, int n) {
    int done = 0;
    while (done < n) {
        const int m = std::min(kSubBlock, n - done);
        updateControls(m);
        const double* p = smooth_.data();

        // Sub-oscillator rate with touch speed from the highest pressure (single SP line).
        double pmax = 0.0;
        for (const auto& v : voices_) if (v.gate) pmax = std::max(pmax, v.pressure);
        const double subHz = 0.1 * std::pow(2000.0, clampd(p[P_SUB_SPEED] + 0.4 * p[P_TOUCH_SPEED] * pmax, 0.0, 1.0));
        const int subFunc = static_cast<int>(params_[P_SUB_FUNC]);
        const double subSmoothC = 1.0 - std::exp(-1.0 / (0.0005 * fsOs_));
        const double subNoiseC = 1.0 - std::exp(-kTwoPi * subHz / fsOs_);

        // Ring modulator (spec 03 §9).
        const double rmA = attackCoeff(attackTimeSec(p[P_RM_ATTACK]), fsOs_);
        const double rmD = decayCoeff(decayTimeSec(p[P_RM_DECAY]), fsOs_);
        const double rmMod = p[P_RM_MOD];

        for (int i = 0; i < m; ++i) {
            double os[kOversample];
            for (int k = 0; k < kOversample; ++k) {
                updateFilters_ = (k == 0);
                const double noise = noiseRng_.bipolar() * 1.2;
                // Sub-oscillator.
                subPhase_ += subHz / fsOs_;
                if (subPhase_ >= 1.0) {
                    subPhase_ -= 1.0;
                    subRandom_ = rng_.bipolar();
                }
                double raw;
                switch (subFunc) {
                    case 1: raw = 2.0 * subPhase_ - 1.0; break;
                    case 2: raw = 1.0 - 2.0 * subPhase_; break;
                    case 3: raw = subPhase_ < 0.5 ? 1.0 : -1.0; break;
                    case 4: raw = subRandom_; break;
                    case 5:
                        subNoiseState_ += (subRandom_ - subNoiseState_) * subNoiseC;
                        raw = subNoiseState_;
                        break;
                    default: raw = std::sin(kTwoPi * subPhase_); break;
                }
                subOut_ += (raw - subOut_) * subSmoothC;

                double bus = 0.0;
                for (int vi = 0; vi < kNumVoices; ++vi) {
                    Voice& v = voices_[vi];
                    if (!v.active() || !((voiceMask_ >> vi) & 1u)) continue;
                    bus += renderVoiceSample(v, vi, noise, subOut_);
                }
                bus *= kBusGain;

                // Ring modulator on the mono sum.
                if (rmAttack_) {
                    rmEnv_ += (1.0 + kAttackOvershoot - rmEnv_) * rmA;
                    if (rmEnv_ >= 1.0) { rmEnv_ = 1.0; rmAttack_ = false; }
                } else {
                    rmEnv_ += (0.0 - rmEnv_) * rmD;
                }
                const double rmHz = 200.0 * clampd(p[P_RM_SPEED] + p[P_RM_DEPTH] * rmEnv_, 0.0, 2.0);
                rmPhase_ += rmHz / fsOs_;
                if (rmPhase_ >= 1.0) rmPhase_ -= 1.0;
                if (rmMod > 1e-5) {
                    const double car = std::sin(kTwoPi * rmPhase_);
                    bus = bus * (1.0 - rmMod) + rmMod * (std::tanh(1.5 * bus * car) / 1.5 + 0.003 * car);
                }
                os[k] = bus;
            }
            mono_[i] = static_cast<float>(decimator_.process(os[0], os[1]) * params_[P_EXPRESSION]);
        }

        // Chorus / tremolo (stereo), reverb, volume, soft clip.
        chorus_.process(mono_.data(), wetL_.data(), wetR_.data(), m,
                        params_[P_CHORUS] >= 0.5, params_[P_TREMOLO] >= 0.5, p[P_FX_SPEED], p[P_FX_DEPTH]);
        reverb_.process(wetL_.data(), wetR_.data(), m, p[P_REV_MIX], p[P_REV_DECAY], p[P_REV_TONE], p[P_REV_PREDELAY]);
        const double vol = p[P_VOLUME] * p[P_VOLUME] * 0.8 * 2.0;
        for (int i = 0; i < m; ++i) {
            const double l = std::tanh(wetL_[i] * vol);
            const double r = std::tanh(wetR_[i] * vol);
            if (outL) outL[done + i] = static_cast<float>(l);
            if (outR) outR[done + i] = static_cast<float>(r);
        }
        done += m;
    }
}

} // namespace tannhauser
