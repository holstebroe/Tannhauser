#ifndef TEARWASH_TEARWASH_HPP
#define TEARWASH_TEARWASH_HPP

// Tearwash 225 engine (docs/tearwash/01 §2): host-rate stereo in, four outputs A–D at the host
// rate. Input pre-emphasis → resample to the core rate → converter (round to 16 bits, gain-ranged
// quantisation) → program network → DAC quantisation → resample to the host rate →
// de-emphasis. No allocation in process(); setSampleRate() and setProgram() allocate.

#include "Control.hpp"
#include "Programs.hpp"
#include "Resampler.hpp"

#include <memory>

namespace tearwash {

// First-order shelf (1 + s·τz)/(1 + s·τp), bilinear with the warping matched at 8 kHz.
class Shelf {
public:
    void init(double fs, double tauZero, double tauPole) {
        const double kPi = 3.14159265358979323846;
        const double wm = 2.0 * kPi * std::min(8000.0, fs / 4.0);
        const double k = wm / std::tan(wm / (2.0 * fs));
        const double den = 1.0 + tauPole * k;
        b0_ = (1.0 + tauZero * k) / den;
        b1_ = (1.0 - tauZero * k) / den;
        a1_ = (1.0 - tauPole * k) / den;
        x1_ = y1_ = 0.0;
    }
    void reset() { x1_ = y1_ = 0.0; }
    inline double process(double x) {
        const double y = b0_ * x + b1_ * x1_ - a1_ * y1_;
        x1_ = x;
        y1_ = y;
        return y;
    }
private:
    double b0_ = 1.0, b1_ = 0.0, a1_ = 0.0, x1_ = 0.0, y1_ = 0.0;
};

class Engine {
public:
    static constexpr double kMasterHz = 30720000.0, kTicksPerStep = 9.0;   // 224X / XL timing

    Engine();
    void setSampleRate(double fs);
    // Takes ownership. Resets the delay memory and the resamplers.
    void setProgram(std::unique_ptr<Program> p);
    Program* program() { return prog_.get(); }
    void reset();
    // in: L, R; out: up to 4 channels (A, B, C, D), any may be null.
    void process(const float* inL, const float* inR, float* const* out, int n);
    // Panel registers (03 §3): sliders through the program's laws, CHORUS → modulation speed,
    // option 40 → Mode Enhancement. Predelay moves are ramped as on the original. Not real-time
    // safe across a program change; safe for register changes of the current program.
    void setControls(const XlRegs& r);
    const XlRegs& controls() const { return regs_; }
    // Mode Enhancement (delay modulation), on by default as in the factory programs.
    void setModeEnhancement(bool on) { modeEnh_ = on; regs_.options = on ? (regs_.options | 0x40) : (regs_.options & ~0x40); }
    double coreRate() const { return coreRate_; }
    // Fixed latency from input to output in host samples.
    double latency() const { return latency_; }

private:
    void configure();
    std::unique_ptr<Program> prog_;
    CoreState state_;
    ModWalker walker_;
    DecayOptimiser decayOpt_;
    bool modeEnh_ = true;
    XlRegs regs_;
    // Predelay ramp [D]: the predelayed input's gain steps to 0, the offset moves, it steps back.
    void controlTick();
    int rampTarget_ = 0;            // pending offsets apply when the gain reaches 0
    uint16_t pendPreL_ = 0, pendPreR_ = 0, curPreL_ = 0, curPreR_ = 0;
    int preGain_ = 16, preHome_ = 16;
    bool preMoving_ = false;
    double ctlPhase_ = 0.0, ctlInterval_ = 32.0;
    StreamResampler<2> in_;
    StreamResampler<4> out_;
    Shelf pre_[2], de_[4];
    // Host-rate output FIFO: the resamplers deliver in phase-dependent bursts, so outputs are
    // queued and released one per host sample after a fixed slack.
    static constexpr int kFifo = 64, kSlack = 4;
    float fifo_[kFifo][4] = {};
    int head_ = 0, count_ = 0;
    bool primed_ = false;
    double fs_ = 48000.0, coreRate_ = 32507.94, latency_ = 0.0;
};

} // namespace tearwash

#endif
