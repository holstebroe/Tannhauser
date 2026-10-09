#ifndef TANNHAUSER_ENVELOPE_HPP
#define TANNHAUSER_ENVELOPE_HPP

// IG00152 filter envelope (IL/AL around the cutoff slider) and IG00159 VCA
// ADSR (spec 02 §5, §6). Analog RC segments: the attack charges toward an
// overshooting target and stops at the peak; decay and release are
// exponential with tau = T / 2.3 (T = time to 10 %). Retriggers start from
// the current value. Times are passed in seconds per call (they may change
// at any time, e.g. the sustain pedal extends the release).

#include <cmath>

namespace tannhauser {

constexpr double kAttackOvershoot = 0.3;
constexpr double kAttackLn = 1.4663370687934272;   // ln(1.3 / 0.3): RC time to reach the peak
constexpr double kDecayLn = 2.302585092994046;     // ln(10)

inline double rcCoeff(double tauSec, double fs) {
    return (tauSec > 0.0 && fs > 0.0) ? 1.0 - std::exp(-1.0 / (tauSec * fs)) : 1.0;
}
// Coefficients for an attack time and a decay/release time (seconds).
inline double attackCoeff(double sec, double fs) { return rcCoeff(sec / kAttackLn, fs); }
inline double decayCoeff(double sec, double fs) { return rcCoeff(sec / kDecayLn, fs); }

// Output in volts: -5*il .. +5*al, rest 0 V.
class FilterEnvelope {
public:
    enum Stage { Idle, Attack, Decay, Release };
    void reset() { v_ = 0.0; stage_ = Idle; }
    void gateOn(double il, double al) {
        il_ = il; al_ = al;
        if (stage_ == Idle || stage_ == Release) {
            // A new key starts at the IL level unless the line is still
            // sounding (then from the current value: analog retrigger).
            if (stage_ == Idle) v_ = -5.0 * il_;
        }
        start_ = v_;
        stage_ = Attack;
    }
    void gateOff() { if (stage_ != Idle) stage_ = Release; }
    void setLevels(double il, double al) { il_ = il; al_ = al; }
    double value() const { return v_; }
    Stage stage() const { return stage_; }

    // Per-sample coefficients from envCoeffs().
    inline double tick(double ca, double cd, double cr) {
        switch (stage_) {
            case Idle:
                v_ = -5.0 * il_;
                break;
            case Attack: {
                const double peak = 5.0 * al_;
                if (v_ >= peak) { stage_ = Decay; break; }
                const double target = peak + kAttackOvershoot * std::fabs(peak - start_) + 1e-3;
                v_ += (target - v_) * ca;
                if (v_ >= peak) { v_ = peak; stage_ = Decay; }
                break;
            }
            case Decay:
                v_ += (0.0 - v_) * cd;
                break;
            case Release: {
                const double floorV = -5.0 * il_;
                v_ += (floorV - v_) * cr;
                break;
            }
        }
        return v_;
    }
private:
    double v_ = 0.0, start_ = 0.0, il_ = 0.0, al_ = 0.0;
    Stage stage_ = Idle;
};

// 0..1 ADSR.
class AmpEnvelope {
public:
    enum Stage { Idle, Attack, Decay, Release };
    void reset() { v_ = 0.0; stage_ = Idle; }
    void gateOn() { stage_ = Attack; }
    void gateOff() { if (stage_ != Idle) stage_ = Release; }
    bool active() const { return stage_ != Idle; }
    Stage stage() const { return stage_; }
    double value() const { return v_; }

    inline double tick(double ca, double cd, double s, double cr) {
        switch (stage_) {
            case Idle: v_ = 0.0; break;
            case Attack:
                v_ += (1.0 + kAttackOvershoot - v_) * ca;
                if (v_ >= 1.0) { v_ = 1.0; stage_ = Decay; }
                break;
            case Decay:
                v_ += (s - v_) * cd;
                break;
            case Release:
                v_ += (0.0 - v_) * cr;
                if (v_ < 1e-5) { v_ = 0.0; stage_ = Idle; }
                break;
        }
        return v_;
    }
private:
    double v_ = 0.0;
    Stage stage_ = Idle;
};

} // namespace tannhauser

#endif
