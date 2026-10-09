#include "Effects.hpp"
#include <algorithm>
#include <cmath>
#include <iterator>

namespace tannhauser {

// --- Half-band decimator (declared in Dsp.hpp) --------------------------------

static double besselI0(double x) {
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 30; ++k) {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
    }
    return sum;
}

HalfbandDecimator::HalfbandDecimator() {
    const int m = kTaps - 1;
    const double beta = 8.0;
    double sum = 0.0;
    for (int n = 0; n < kTaps; ++n) {
        const double t = n - m / 2.0;
        const double sinc = (t == 0.0) ? 0.5 : std::sin(0.5 * kPi * t) / (kPi * t);
        const double r = 2.0 * n / m - 1.0;
        const double w = besselI0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / besselI0(beta);
        h_[n] = sinc * w;
        sum += h_[n];
    }
    for (double& c : h_) c /= sum;
    reset();
}

void HalfbandDecimator::reset() {
    std::fill(std::begin(buf_), std::end(buf_), 0.0);
    pos_ = 0;
}

void HalfbandDecimator::push(double x) {
    // Mirrored buffer: every window of kTaps is contiguous.
    buf_[pos_] = x;
    buf_[pos_ + kTaps] = x;
    pos_ = (pos_ + 1) % kTaps;
}

double HalfbandDecimator::process(double x0, double x1) {
    push(x0);
    push(x1);
    const double* w = buf_ + pos_;   // oldest .. newest
    double acc = h_[kTaps / 2] * w[kTaps / 2];
    // Half-band: every other tap away from the centre is zero.
    for (int n = (kTaps / 2) % 2 == 0 ? 1 : 0; n < kTaps; n += 2) {
        if (n == kTaps / 2) continue;
        acc += h_[n] * w[n];
    }
    return acc;
}

// --- Delay line ---------------------------------------------------------------

void DelayLine::allocate(size_t minSize) {
    size_t n = 1;
    while (n < minSize + 4) n <<= 1;
    buf_.assign(n, 0.0);
    mask_ = n - 1;
    pos_ = 0;
}

void DelayLine::clear() { std::fill(buf_.begin(), buf_.end(), 0.0); }

double DelayLine::tapFrac(double d) const {
    if (d < 1.0) d = 1.0;
    const size_t i = static_cast<size_t>(d);
    const double f = d - static_cast<double>(i);
    const double ym1 = tap(i - 1 > 0 ? i - 1 : 1), y0 = tap(i), y1 = tap(i + 1), y2 = tap(i + 2);
    const double c0 = y0;
    const double c1 = 0.5 * (y1 - ym1);
    const double c2 = ym1 - 2.5 * y0 + 2.0 * y1 - 0.5 * y2;
    const double c3 = 0.5 * (y2 - ym1) + 1.5 * (y0 - y1);
    return ((c3 * f + c2) * f + c1) * f + c0;
}

// --- Chorus / tremolo -----------------------------------------------------------

void ChorusTremolo::setSampleRate(double fs) {
    fs_ = fs;
    line_.allocate(static_cast<size_t>(0.05 * fs) + 8);
    // BBD band limit: the MN3001 path is filtered before and after (spec 03 §11).
    inLp_.setLowpass(8000.0, 0.707, fs);
    outLpA_.setLowpass(8000.0, 0.707, fs);
    outLpB_.setLowpass(8000.0, 0.707, fs);
    reset();
}

void ChorusTremolo::reset() {
    line_.clear();
    inLp_.reset(); outLpA_.reset(); outLpB_.reset();
    lfoPhase_ = vibPhase_ = tremPhase_ = 0.0;
    depthSm_ = mixSm_ = tremSm_ = 0.0;
}

void ChorusTremolo::process(const float* in, float* outL, float* outR, int n,
                            bool chorusOn, bool tremoloOn, double speed, double depth) {
    const double chorusRate = 0.2 * std::pow(40.0, speed);
    const double tremRate = 0.5 * std::pow(20.0, speed);
    const double depthMs = 0.2 + 3.8 * depth;
    const double sm = onePoleCoeff(0.02, fs_);
    const double mixTarget = chorusOn ? 1.0 : 0.0;
    const double tremTarget = tremoloOn ? depth : 0.0;
    for (int i = 0; i < n; ++i) {
        const double x = in[i];
        mixSm_ += (mixTarget - mixSm_) * sm;
        tremSm_ += (tremTarget - tremSm_) * sm;
        depthSm_ += (depthMs - depthSm_) * sm;
        double l = x, r = x;
        if (mixSm_ > 1e-4) {
            line_.write(inLp_.tick(x) + 1e-4 * rng_.bipolar());   // BBD noise floor ~ -80 dB
            lfoPhase_ += chorusRate / fs_;
            vibPhase_ += 6.0 / fs_;
            if (lfoPhase_ >= 1.0) lfoPhase_ -= 1.0;
            if (vibPhase_ >= 1.0) vibPhase_ -= 1.0;
            const double lfo = std::sin(kTwoPi * lfoPhase_) + 0.15 * std::sin(kTwoPi * vibPhase_);
            const double dA = (0.007 + 0.001 * depthSm_ * lfo) * fs_;
            const double dB = (0.011 - 0.001 * depthSm_ * lfo) * fs_;
            const double a = outLpA_.tick(line_.tapFrac(dA));
            const double b = outLpB_.tick(line_.tapFrac(dB));
            l = x * (1.0 - 0.3 * mixSm_) + 0.7 * mixSm_ * a;
            r = x * (1.0 - 0.3 * mixSm_) + 0.7 * mixSm_ * b;
        } else {
            line_.write(0.0);
        }
        if (tremSm_ > 1e-4 || tremoloOn) {
            tremPhase_ += tremRate / fs_;
            if (tremPhase_ >= 1.0) tremPhase_ -= 1.0;
            const double s = std::sin(kTwoPi * tremPhase_);
            l *= 1.0 - tremSm_ * (0.5 + 0.5 * s);
            r *= 1.0 - tremSm_ * (0.5 - 0.5 * s);
        }
        outL[i] = static_cast<float>(l);
        outR[i] = static_cast<float>(r);
    }
}

// --- Plate reverb -----------------------------------------------------------------

size_t PlateReverb::sc(double s) const {
    return std::max<size_t>(1, static_cast<size_t>(s * scale_ + 0.5));
}

void PlateReverb::setSampleRate(double fs) {
    fs_ = fs;
    scale_ = fs / 29761.0;
    pre_.allocate(static_cast<size_t>(0.16 * fs) + 4);
    const double inLens[4] = { 142, 107, 379, 277 };
    for (int i = 0; i < 4; ++i) {
        in_[i].len = sc(inLens[i]);
        in_[i].d.allocate(in_[i].len + 2);
    }
    const double modLens[2] = { 672, 908 }, d1[2] = { 4453, 4217 }, ap2[2] = { 1800, 2656 }, d2[2] = { 3720, 3163 };
    for (int k = 0; k < 2; ++k) {
        apModLen_[k] = sc(modLens[k]);
        apMod_[k].allocate(apModLen_[k] + sc(40) + 4);
        del1Len_[k] = sc(d1[k]);
        del1_[k].allocate(del1Len_[k] + 2);
        ap2_[k].len = sc(ap2[k]);
        ap2_[k].d.allocate(ap2_[k].len + 2);
        del2Len_[k] = sc(d2[k]);
        del2_[k].allocate(del2Len_[k] + 2);
    }
    reset();
}

void PlateReverb::reset() {
    pre_.clear();
    bandwidth_.reset();
    for (auto& a : in_) a.d.clear();
    for (int k = 0; k < 2; ++k) {
        apMod_[k].clear(); del1_[k].clear(); ap2_[k].d.clear(); del2_[k].clear();
        damp_[k] = 0.0; fb_[k] = 0.0;
    }
    modPhase_ = 0.0;
    mixSm_ = 0.0;
}

void PlateReverb::process(float* L, float* R, int n, double mix, double decay, double tone, double predelay) {
    const double sm = onePoleCoeff(0.05, fs_);
    if (mix <= 0.0 && mixSm_ < 1e-5) { mixSm_ = 0.0; return; }
    const double g = 0.2 + 0.78 * clampd(decay, 0.0, 1.0);          // tank feedback
    const double dampCoef = 0.05 + 0.65 * (1.0 - clampd(tone, 0.0, 1.0));
    bandwidth_.setCutoff(2000.0 + 14000.0 * tone, fs_);
    const double preSamples = std::max(1.0, predelay * 0.15 * fs_);
    const double exc = 12.0 * scale_;
    const double modInc = 0.9 / fs_;
    const size_t tapsL[7] = { sc(266), sc(2974), sc(1913), sc(1996), sc(1990), sc(187), sc(1066) };
    const size_t tapsR[7] = { sc(353), sc(3627), sc(1228), sc(2673), sc(2111), sc(335), sc(121) };
    for (int i = 0; i < n; ++i) {
        mixSm_ += (mix - mixSm_) * sm;
        const double x = 0.5 * (L[i] + R[i]);
        pre_.write(x);
        double v = bandwidth_.tick(pre_.tapFrac(preSamples));
        v = in_[0].tick(v, 0.75);
        v = in_[1].tick(v, 0.75);
        v = in_[2].tick(v, 0.625);
        v = in_[3].tick(v, 0.625);
        modPhase_ += modInc;
        if (modPhase_ >= 1.0) modPhase_ -= 1.0;
        const double mods[2] = { std::sin(kTwoPi * modPhase_), std::cos(kTwoPi * modPhase_) };
        const double feed[2] = { fb_[1], fb_[0] };   // each half is fed by the other
        for (int k = 0; k < 2; ++k) {
            double t = v + g * feed[k];
            // Modulated allpass (decay diffusion 1 = -0.7).
            const double delayed = apMod_[k].tapFrac(static_cast<double>(apModLen_[k]) + exc * mods[k]);
            const double w = t - 0.7 * delayed;   // g = -0.7
            apMod_[k].write(w);
            t = delayed + 0.7 * w;
            del1_[k].write(t);
            t = del1_[k].tap(del1Len_[k]);
            damp_[k] += (t - damp_[k]) * (1.0 - dampCoef);
            t = damp_[k] * g;
            t = ap2_[k].tick(t, 0.5);
            del2_[k].write(t);
            fb_[k] = del2_[k].tap(del2Len_[k]);
        }
        // Output taps (Dattorro's table, half 0 = "left" tank, 1 = "right").
        double wl = del1_[1].tap(tapsL[0]) + del1_[1].tap(tapsL[1]) - ap2_[1].d.tap(tapsL[2])
                  + del2_[1].tap(tapsL[3]) - del1_[0].tap(tapsL[4]) - ap2_[0].d.tap(tapsL[5])
                  - del2_[0].tap(tapsL[6]);
        double wr = del1_[0].tap(tapsR[0]) + del1_[0].tap(tapsR[1]) - ap2_[0].d.tap(tapsR[2])
                  + del2_[0].tap(tapsR[3]) - del1_[1].tap(tapsR[4]) - ap2_[1].d.tap(tapsR[5])
                  - del2_[1].tap(tapsR[6]);
        wl *= 0.6; wr *= 0.6;
        L[i] = static_cast<float>(L[i] * (1.0 - 0.4 * mixSm_) + mixSm_ * wl);
        R[i] = static_cast<float>(R[i] * (1.0 - 0.4 * mixSm_) + mixSm_ * wr);
    }
}

} // namespace tannhauser
