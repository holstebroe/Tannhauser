#include "Tearwash.hpp"

#include <cmath>

namespace tearwash {

Engine::Engine() : prog_(std::make_unique<ConcertHall>()) { configure(); }

void Engine::setSampleRate(double fs) {
    fs_ = fs;
    configure();
}

void Engine::setProgram(std::unique_ptr<Program> p) {
    prog_ = std::move(p);
    configure();
}

void Engine::configure() {
    coreRate_ = kMasterHz / kTicksPerStep / prog_->loopLength();
    const double dIn = in_.setup(fs_, coreRate_);          // host samples
    const double dOut = out_.setup(coreRate_, fs_);        // core samples
    latency_ = dIn + dOut * fs_ / coreRate_ + kSlack;
    // 224X converter emphasis: +12 dB shelf (50 µs / 12.5 µs) and its inverse (02 §1).
    for (auto& s : pre_) s.init(fs_, 50e-6, 12.5e-6);
    for (auto& s : de_) s.init(fs_, 12.5e-6, 50e-6);
    walker_.setup(coreRate_, prog_->modTaps(), 980.0, 4, prog_->modSegment(), prog_->modWindow());
    for (int i = 0; i < prog_->modTaps(); ++i) walker_.setHome(i, prog_->modHome(i));
    ctlInterval_ = coreRate_ / 1000.0;      // 1 kHz control tick
    decayOpt_.setup(coreRate_);
    regs_ = prog_->factory();
    reset();
    setControls(regs_);
}

void Engine::setControls(const XlRegs& r) {
    regs_ = r;
    modeEnh_ = (r.options & 0x40) != 0;
    int divider, step4;
    law::chorus(r.at(3, 3), divider, step4);
    walker_.setSpeed(coreRate_, 980.0 / divider, step4);
    decayOpt_.setMid(law::step5(r.at(1, 2) > 0xF9 ? 0xF9 : r.at(1, 2)));
    uint16_t* pl = nullptr;
    uint16_t* pr = nullptr;
    int* gain = nullptr;
    prog_->predelayRamp(pl, pr, gain);
    if (!pl) { prog_->applyControls(r); return; }
    const uint16_t oldL = *pl, oldR = *pr;
    prog_->applyControls(r);
    preHome_ = *gain;
    // Keep the old predelay until the ramp has taken the gain to zero.
    if (*pl != oldL || *pr != oldR) {
        pendPreL_ = *pl; pendPreR_ = *pr;
        *pl = oldL; *pr = oldR;
        *gain = preGain_;
        preMoving_ = true;
    } else {
        *gain = preGain_;
    }
}

void Engine::controlTick() {
    uint16_t* pl = nullptr;
    uint16_t* pr = nullptr;
    int* gain = nullptr;
    prog_->predelayRamp(pl, pr, gain);
    if (!pl || (!preMoving_ && preGain_ == preHome_)) return;
    if (preMoving_) {
        if (preGain_ > 0) --preGain_;
        else { *pl = pendPreL_; *pr = pendPreR_; preMoving_ = false; }
    } else if (preGain_ < preHome_) {
        ++preGain_;
    } else if (preGain_ > preHome_) {
        --preGain_;
    }
    *gain = preGain_;
}

void Engine::reset() {
    state_.clear();
    prog_->reset();
    in_.reset();
    out_.reset();
    for (auto& s : pre_) s.reset();
    for (auto& s : de_) s.reset();
    head_ = count_ = 0;
    primed_ = false;
    walker_.reset();
    decayOpt_.reset();
    prog_->setDecayReduction(0);
    // A pending predelay move completes at once: nothing is playing to click.
    uint16_t* pl = nullptr;
    uint16_t* pr = nullptr;
    int* gain = nullptr;
    prog_->predelayRamp(pl, pr, gain);
    if (pl && preMoving_) { *pl = pendPreL_; *pr = pendPreR_; }
    preMoving_ = false;
    preGain_ = preHome_;
    if (gain) *gain = preHome_;
    ctlPhase_ = 0.0;
}

static inline int16_t toWord(float x) {
    const long v = std::lround(double(x) * 32768.0);
    return static_cast<int16_t>(v < -32768 ? -32768 : (v > 32767 ? 32767 : v));
}

void Engine::process(const float* inL, const float* inR, float* const* out, int n) {
    for (int i = 0; i < n; ++i) {
        const float x[2] = { static_cast<float>(pre_[0].process(inL[i])), static_cast<float>(pre_[1].process(inR[i])) };
        in_.push(x);
        float c[2];
        while (in_.pull(c)) {
            state_.inL = fpcQuantize(toWord(c[0]));
            state_.inR = fpcQuantize(toWord(c[1]));
            if ((ctlPhase_ += 1.0) >= ctlInterval_) { ctlPhase_ -= ctlInterval_; controlTick(); }
            if (decayOpt_.tick(state_.inL, state_.inR, (regs_.options & 0x80) != 0, prog_->decayReductionMax()))
                prog_->setDecayReduction(decayOpt_.reduction());
            if (modeEnh_ && walker_.tick())
                for (int k = 0; k < prog_->modTaps(); ++k) {
                    uint16_t o0, o1;
                    int w;
                    walker_.tap(k, o0, o1, w);
                    prog_->setModTap(k, o0, o1, w);
                }
            prog_->tick(state_);
            float d[4];
            for (int k = 0; k < 4; ++k) d[k] = fpcQuantize(state_.dac[k]) * (1.0f / 32768.0f);
            out_.push(d);
        }
        float y[4] = { 0, 0, 0, 0 };
        while (count_ < kFifo && out_.pull(fifo_[(head_ + count_) % kFifo])) ++count_;
        if (!primed_ && count_ > kSlack) primed_ = true;
        if (primed_ && count_ > 0) {
            for (int k = 0; k < 4; ++k) y[k] = fifo_[head_][k];
            head_ = (head_ + 1) % kFifo;
            --count_;
        }
        for (int k = 0; k < 4; ++k)
            if (out[k]) out[k][i] = static_cast<float>(de_[k].process(y[k]));
    }
}

} // namespace tearwash
