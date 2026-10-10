#ifndef TANNHAUSER_SYNTH_ENGINE_HPP
#define TANNHAUSER_SYNTH_ENGINE_HPP

// The CS-80 engine: 8 voices x 2 lines (16 voice cards), key assigner,
// global modulators, mono bus with ring modulator, chorus/tremolo, reverb.
// See docs/spec/02_VOICE.md and 03_GLOBAL.md; every constant here is
// documented there with its evidence tag.

#include "Params.hpp"
#include "Dsp.hpp"
#include "Envelope.hpp"
#include "Effects.hpp"
#include <array>
#include <vector>

namespace tannhauser {

constexpr int kNumVoices = 8;
constexpr int kMaxOversample = 4;   // 2x normally, 4x with the os.4x switch (plan 1.10)
constexpr int kSubBlock = 16;   // host samples per control update

// Per-card calibration record (spec 02 §8), at Drift = 1.
struct CardCalibration {
    double scaleErr = 0.0;     // relative VCO scale error
    double offsetHz = 0.0;     // VCO offset in Hz
    double pulseWidth = 1.0;   // saw start-pulse width factor
    double pulseHeight = 1.0;  // saw start-pulse height factor
    double cutScale = 0.0;     // relative cutoff error
    double qScale = 0.0;       // relative Q error
    double envScale = 0.0;     // relative envelope time error
    double feedthrough = 0.0;  // VCA bleed (linear gain)
    double phase0 = 0.0;
    double jitter = 1.0;       // cycle-to-cycle period jitter factor
};

// Smoothed per-line control values for one sub-block (shared by the 8 cards of a line).
struct LineControls {
    double feetRatio = 1.0;
    double pwBase = 0.5, pwmDepth = 0.0;
    double sawOn = 1.0, squareOn = 0.0, noise = 0.0;
    double hpfV = 0.0, lpfV = 7.0;
    double vqH = 10.0, vqL = 8.0;
    double il = 0.0, al = 0.3;
    double fegA = 0.03, fegD = 0.3, fegR = 0.2;   // seconds
    double vcfLevel = 1.0, sine = 0.0;
    double vegA = 0.003, vegD = 0.3, vegS = 0.8, vegR = 0.2;
    double level = 0.64;
    double initBrill = 0.3, initLevel = 0.5, afterBrill = 0.3, afterLevel = 0.3;
    double pwmLfo = 0.0;   // this sub-block's PWM LFO value
    double mixGain = 1.0;
    bool silent = false;   // Level or Mix at 0: the line is not rendered
};

struct LineState {
    CardCalibration cal;
    OuProcess drift;
    double phase = 0.0;
    double jitter = 1.0;       // this cycle's frequency factor (spec 02 §8)
    double blep[4]{};          // VCO output n-2 .. n+1 with band-limited step corrections
    double phaseHist[2]{};     // phase at n-1, n-2 (sine aligned with the delayed saw/pulse)
    bool pulseHigh = false;
    OtaSvf hp, lp;
    OnePoleLp pole;
    FilterEnvelope feg;
    AmpEnvelope veg;
    double noiseLevel = 0.0;   // ~1 ms smoothed level
    // Control-rate coefficients (updateControls).
    double fegCa = 1.0, fegCd = 1.0, fegCr = 1.0;
    double vegCa = 1.0, vegCd = 1.0, vegCr = 1.0;
    double qaH = 0.5, qaL = 0.5;
    double gain = 0.0;           // dynamics x level x keyboard level x mix, per host sample
};

struct Voice {
    int key = -1;
    bool gate = false;
    bool fastRelease = false;      // Sustain II: silenced by a new note
    uint64_t onOrder = 0, offOrder = 0;
    double velocity = 0.0;
    double pressureTarget = 0.0, pressure = 0.0;
    double keySemis = 60.0;        // target key (MIDI note number)
    double rawSemis = 60.0;        // glide/gliss position before smoothing
    double glissStart = 60.0;
    double glissClock = 0.0;
    double smoothSemis = 60.0;     // key voltage after the YM26700 low-pass
    double scoop = 0.0;            // initial pitch bend, semitones
    double ribbonBase = 0.0;       // ribbon bend kept from earlier touches minus this touch's start
    double ribbonTarget = 0.0;     // this voice's ribbon bend, semitones (spec 03 §7)
    double ribbonSm = 0.0;         // ... smoothed ~2 ms
    double kbdBrillV = 0.0, kbdLevel = 1.0;
    double fKv = 261.6, fVco = 261.6;   // updated once per host sample
    LineState line[2];
    bool active() const { return line[0].veg.active() || line[1].veg.active(); }
};

class SynthEngine {
public:
    SynthEngine();

    void setSampleRate(double fs);
    double sampleRate() const { return fs_; }
    void reset();

    // Parameters: set from the audio thread (events) or before processing.
    void setParam(uint32_t id, double value);
    double getParam(uint32_t id) const { return params_[id < PARAM_COUNT ? id : 0]; }

    void noteOn(int key, double velocity);
    void noteOff(int key);
    void notePressure(int key, double pressure);
    void channelPressure(double pressure);
    void pitchBend(double norm);        // -1..1
    void modWheel(double v);            // 0..1
    void allNotesOff();
    void allSoundOff();

    void process(float* outL, float* outR, int n);

    // Tests / GUI probes.
    int activeVoiceCount() const;
    const Voice& voice(int i) const { return voices_[i]; }
    double ringModEnvelope() const { return rmEnv_; }
    // Frequency (Hz) the VCO of voice v, line l would run at now.
    double lineFrequency(int v, int l) const;
    // Cutoff voltages of voice v, line l at the last processed sample.
    double lastVfL(int v, int l) const { return lastVfL_[v][l]; }
    double lastVfH(int v, int l) const { return lastVfH_[v][l]; }
    void setCalibrationSeed(uint64_t seed);
    // Lines are rendered with this voice mask (tests); default all.
    void setVoiceMask(uint32_t mask) { voiceMask_ = mask; }

private:
    double fs_ = 48000.0, fsOs_ = 96000.0;
    int osFactor_ = 2;
    double noiseScale_ = 1.0;   // keeps the noise density independent of fsOs_
    std::array<double, PARAM_COUNT> params_{};
    std::array<double, PARAM_COUNT> smooth_{};   // control-rate smoothed copy
    bool smoothInit_ = false;

    std::array<Voice, kNumVoices> voices_;
    uint64_t noteCounter_ = 0;
    int nextVoice_ = 0;
    int heldKeys_ = 0;
    double lastPlayedSemis_ = 60.0;
    bool havePlayed_ = false;

    // Performance state.
    double bendNorm_ = 0.0, modWheel_ = 0.0;
    double ribbonSm_ = 0.0;        // ribbon moved without a touch (host automation): all voices
    bool ribbonTouched_ = false;

    // Global modulators.
    Rng rng_{0x7A5E};
    Rng noiseRng_{0x5EED};
    double subPhase_ = 0.0, subRandom_ = 0.0, subNoiseState_ = 0.0, subOut_ = 0.0;
    double pwmPhase_[2]{ 0.0, 0.37 };
    LineControls lc_[2];

    // Ring modulator.
    double rmEnv_ = 0.0;
    bool rmAttack_ = false;
    double rmPhase_ = 0.0;

    // Bus.
    HalfbandDecimator decimator_;      // 2x -> 1x
    HalfbandDecimator decimator4_;     // 4x -> 2x (os.4x)
    ChorusTremolo chorus_;
    PlateReverb reverb_;
    std::vector<float> mono_, wetL_, wetR_;

    double lastVfL_[kNumVoices][2]{};
    double lastVfH_[kNumVoices][2]{};
    uint32_t voiceMask_ = 0xFFFFFFFFu;
    uint64_t calibrationSeed_ = 0xC580;
    bool updateFilters_ = true;
    double glideCoeff_ = 1.0, scoopDecay_ = 1.0;   // per oversampled sample

    void initCalibration();
    void configureOversampling(int factor);
    void updateControls(int hostSamples);
    void startVoice(Voice& v, int key, double velocity);
    int allocateVoice(int key);
    double renderVoiceSample(Voice& v, int vi, double noise, double subLfo);
    double portaPeriodSec() const;
};

} // namespace tannhauser

#endif
