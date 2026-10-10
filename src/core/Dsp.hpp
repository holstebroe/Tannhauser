#ifndef TANNHAUSER_DSP_HPP
#define TANNHAUSER_DSP_HPP

// Small DSP building blocks shared by the voice and the bus (spec 02/03).

#include <cmath>
#include <cstdint>
#include <vector>

namespace tannhauser {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

inline double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

// One-pole smoothing coefficient for time constant tau (s) at rate fs.
inline double onePoleCoeff(double tauSec, double fs) {
    if (!(tauSec > 0.0) || !(fs > 0.0)) return 1.0;
    return 1.0 - std::exp(-1.0 / (tauSec * fs));
}

inline double wrap01(double x) { return x - std::floor(x); }

// 4-point band-limited step residual (spec 02 §2): the step smoothed by a
// cubic B-spline (support +-2 samples) minus the ideal step. tau = sample time
// relative to the discontinuity, in samples; zero outside (-2, 2). Add
// jump * blep4(tau) to the four samples around each jump.
inline double blep4Rise(double u) {   // integral of the cubic B-spline from -2 to u - 2, u in [0, 2]
    if (u < 1.0) {
        const double u2 = u * u;
        return u2 * u2 * (1.0 / 24.0);
    }
    const double x = u - 2.0;   // [-1, 0]
    const double x2 = x * x;
    return 0.5 + x * (4.0 - 2.0 * x2 - 0.75 * x2 * x) * (1.0 / 6.0);
}
inline double blep4(double tau) {
    if (tau <= -2.0 || tau >= 2.0) return 0.0;
    return tau < 0.0 ? blep4Rise(tau + 2.0) : -blep4Rise(2.0 - tau);
}

// xorshift64* random numbers: deterministic, allocation-free, audio-thread safe.
class Rng {
public:
    explicit Rng(uint64_t seed = 0x9E3779B97F4A7C15ull) { setSeed(seed); }
    void setSeed(uint64_t seed) { s_ = seed ? seed : 0x9E3779B97F4A7C15ull; }
    uint64_t next() {
        s_ ^= s_ >> 12; s_ ^= s_ << 25; s_ ^= s_ >> 27;
        return s_ * 0x2545F4914F6CDD1Dull;
    }
    double uniform() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }   // [0,1)
    double bipolar() { return 2.0 * uniform() - 1.0; }                                              // [-1,1)
    double gauss() {   // sum of 4 uniforms, unit variance approx.
        return (uniform() + uniform() + uniform() + uniform() - 2.0) * 1.7320508;
    }
private:
    uint64_t s_;
};

// OTA state-variable filter with saturating integrators (spec 02 §4, plan
// 1.5): bp' = w T(u1), lp' = w T(bp), u1 = x - k bp - lp, T(v) = V tanh(v/V).
// Trapezoidal; each sample the OTA gains tanh(v)/v are linearised at an
// estimate of v and the linear TPT system is solved exactly, twice: predict
// with the gains carried from the previous sample, correct with the gains at
// the predicted voltages (which are carried on). Small signals give exactly
// the linear SVF.
struct OtaSvf {
    double ic1 = 0.0, ic2 = 0.0;
    double g = 0.0, k = 2.0;
    double s1 = 1.0, s2 = 1.0;   // OTA gains tanh(v/V)/(v/V) carried to the next sample
    void reset() { ic1 = ic2 = 0.0; s1 = s2 = 1.0; }
    void setCoeffs(double gIn, double damping) { g = gIn; k = damping; }
    // Both OTA gains with one division.
    static inline void gains(double a, double b, double& ga, double& gb) {
        const double a2 = a * a, b2 = b * b;
        const double na = a2 < 9.0 ? 27.0 + a2 : 1.0, da = a2 < 9.0 ? 27.0 + 9.0 * a2 : std::sqrt(a2);
        const double nb = b2 < 9.0 ? 27.0 + b2 : 1.0, db = b2 < 9.0 ? 27.0 + 9.0 * b2 : std::sqrt(b2);
        const double inv = 1.0 / (da * db);
        ga = na * db * inv;
        gb = nb * da * inv;
    }
    // invV = 1 / OTA saturation voltage. Returns lp/bp/hp through refs.
    inline void tick(double x, double invV, double& lp, double& bp, double& hp) {
        double G1 = g * s1, G2 = g * s2;
        bp = (G1 * (x - ic2) + ic1) / (1.0 + G1 * (k + G2));
        lp = G2 * bp + ic2;
        hp = x - k * bp - lp;
        gains(hp * invV, bp * invV, s1, s2);
        G1 = g * s1; G2 = g * s2;
        bp = (G1 * (x - ic2) + ic1) / (1.0 + G1 * (k + G2));
        lp = G2 * bp + ic2;
        hp = x - k * bp - lp;
        ic1 = 2.0 * bp - ic1;
        ic2 = 2.0 * lp - ic2;
    }
};

// One-pole low-pass (TPT form).
struct OnePoleLp {
    double s = 0.0, G = 1.0;
    void reset() { s = 0.0; }
    void setCutoff(double fc, double fs) {
        const double g = std::tan(kPi * clampd(fc / fs, 1e-6, 0.49));
        G = g / (1.0 + g);
    }
    inline double tick(double x) {
        const double v = (x - s) * G;
        const double y = v + s;
        s = y + v;
        return y;
    }
};

// RBJ biquad, transposed direct form II.
struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0.0; }
    void setLowpass(double fc, double q, double fs) {
        const double w = kTwoPi * clampd(fc / fs, 1e-5, 0.49);
        const double cw = std::cos(w), alpha = std::sin(w) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (1.0 - cw) * 0.5 / a0; b1 = (1.0 - cw) / a0; b2 = b0;
        a1 = -2.0 * cw / a0; a2 = (1.0 - alpha) / a0;
    }
    inline double tick(double x) {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// 2:1 decimator: windowed-sinc half-band FIR (Kaiser), polyphase.
class HalfbandDecimator {
public:
    HalfbandDecimator();
    void reset();
    // Consumes two input samples, returns one output sample.
    double process(double x0, double x1);
    static constexpr int kTaps = 63;   // odd; centre tap at 31
private:
    double h_[kTaps];
    double buf_[2 * kTaps];
    int pos_ = 0;
    void push(double x);
};

// Ornstein-Uhlenbeck random walk (slow drift), updated at control rate.
struct OuProcess {
    double x = 0.0;
    void step(Rng& rng, double dt, double tau, double sigma) {
        x += -x * dt / tau + sigma * std::sqrt(2.0 * dt / tau) * rng.gauss();
    }
};

} // namespace tannhauser

#endif
