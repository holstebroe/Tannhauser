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

// PolyBLEP residual for a discontinuity of size 2 (from -1 to +1 is +2) at
// t = 0, t = position since the discontinuity in [0,1), dt = phase increment.
// Add (jump / 2) * polyBlep(t, dt).
inline double polyBlep(double t, double dt) {
    if (t < dt) {
        const double x = t / dt;
        return x + x - x * x - 1.0;
    }
    if (t > 1.0 - dt) {
        const double x = (t - 1.0) / dt;
        return x * x + x + x + 1.0;
    }
    return 0.0;
}

inline double wrap01(double x) { return x - std::floor(x); }

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

// TPT (Zavalishin / Simper) state-variable filter, trapezoidal integration.
struct Svf {
    double ic1 = 0.0, ic2 = 0.0;
    double a1 = 1.0, a2 = 0.0, a3 = 0.0, k = 2.0;
    void reset() { ic1 = ic2 = 0.0; }
    // g = tan(pi fc / fs), damping k = 1/Q.
    void setCoeffs(double g, double damping) {
        k = damping;
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    // Returns lp/bp/hp through refs.
    inline void tick(double v0, double& lp, double& bp, double& hp) {
        const double v3 = v0 - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
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
