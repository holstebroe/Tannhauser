#ifndef TANNHAUSER_EFFECTS_HPP
#define TANNHAUSER_EFFECTS_HPP

// Bus effects at host rate (spec 03 §11, §12): the BBD-style chorus/tremolo
// (OE1/OE2 defaults, option A) and the added plate reverb.

#include "Dsp.hpp"
#include <vector>

namespace tannhauser {

// Power-of-two circular delay line with fractional (Hermite) reads.
class DelayLine {
public:
    void allocate(size_t minSize);
    void clear();
    inline void write(double x) { buf_[pos_] = x; pos_ = (pos_ + 1) & mask_; }
    // Integer delay in samples (1 = the sample written last).
    inline double tap(size_t d) const { return buf_[(pos_ - d) & mask_]; }
    double tapFrac(double d) const;
private:
    std::vector<double> buf_;
    size_t mask_ = 0, pos_ = 0;
};

class ChorusTremolo {
public:
    void setSampleRate(double fs);
    void reset();
    // speed, depth 0..1. In: mono. Out: stereo.
    void process(const float* in, float* outL, float* outR, int n,
                 bool chorusOn, bool tremoloOn, double speed, double depth);
private:
    double fs_ = 48000.0;
    DelayLine line_;
    Biquad inLp_, outLpA_, outLpB_;
    double lfoPhase_ = 0.0, vibPhase_ = 0.0, tremPhase_ = 0.0;
    double depthSm_ = 0.0, mixSm_ = 0.0, tremSm_ = 0.0;
    Rng rng_{0xBBD0001};
};

// Dattorro (1997) plate topology with modulated tank allpasses.
class PlateReverb {
public:
    void setSampleRate(double fs);
    void reset();
    // Adds the wet signal to L/R in place (dry is kept).
    void process(float* L, float* R, int n, double mix, double decay, double tone, double predelay);
private:
    struct Allpass {
        DelayLine d; size_t len = 1;
        inline double tick(double x, double g) {
            const double delayed = d.tap(len);
            const double w = x + g * delayed;
            d.write(w);
            return delayed - g * w;
        }
    };
    double fs_ = 48000.0, scale_ = 1.0;
    DelayLine pre_;
    OnePoleLp bandwidth_;
    Allpass in_[4];
    DelayLine apMod_[2];
    size_t apModLen_[2]{};
    DelayLine del1_[2], del2_[2];
    size_t del1Len_[2]{}, del2Len_[2]{};
    Allpass ap2_[2];
    double damp_[2]{};
    double fb_[2]{};
    double modPhase_ = 0.0;
    double mixSm_ = 0.0;
    size_t sc(double samplesAt29761) const;
};

} // namespace tannhauser

#endif
