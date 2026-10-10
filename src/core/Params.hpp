#ifndef TANNHAUSER_PARAMS_HPP
#define TANNHAUSER_PARAMS_HPP

// The single source of truth for every parameter: id, text key (used by the
// saved state and the presets), display name, range, default and flags.
// docs/spec/04_PARAMETERS.md mirrors this table. CLAP ids and keys are
// stable: never renumber or rename, only append.

#include <cstdint>
#include <cstddef>
#include <string>

namespace tannhauser {

// Per-line parameter offsets. Line I uses ids 0..26, line II 27..53.
enum LineParam : uint32_t {
    LP_FEET = 0,
    LP_PWM_SPEED,
    LP_PWM_DEPTH,
    LP_PW,
    LP_SQUARE,
    LP_SAW,
    LP_NOISE,
    LP_HPF,
    LP_RES_H,
    LP_LPF,
    LP_RES_L,
    LP_IL,
    LP_AL,
    LP_FEG_A,
    LP_FEG_D,
    LP_FEG_R,
    LP_VCF_LEVEL,
    LP_SINE,
    LP_VEG_A,
    LP_VEG_D,
    LP_VEG_S,
    LP_VEG_R,
    LP_LEVEL,
    LP_INIT_BRILL,
    LP_INIT_LEVEL,
    LP_AFTER_BRILL,
    LP_AFTER_LEVEL,
    LP_COUNT
};

constexpr uint32_t kLine1Base = 0;
constexpr uint32_t kLine2Base = LP_COUNT;
constexpr uint32_t lineParam(int line, LineParam p) { return (line == 0 ? kLine1Base : kLine2Base) + p; }

enum GlobalParam : uint32_t {
    P_VOLUME = 2 * LP_COUNT,   // 54
    P_PITCH,
    P_DETUNE,
    P_MIX,
    P_BRILLIANCE,
    P_RESONANCE,
    P_SUB_FUNC,
    P_SUB_SPEED,
    P_SUB_VCO,
    P_SUB_VCF,
    P_SUB_VCA,
    P_TOUCH_BEND,
    P_TOUCH_SPEED,
    P_TOUCH_VCO,
    P_TOUCH_VCF,
    P_KBD_BRILL_LOW,
    P_KBD_BRILL_HIGH,
    P_KBD_LEVEL_LOW,
    P_KBD_LEVEL_HIGH,
    P_RM_ATTACK,
    P_RM_DECAY,
    P_RM_DEPTH,
    P_RM_SPEED,
    P_RM_MOD,
    P_SUS_MODE,
    P_SUS_TIME,
    P_SUS_PEDAL,
    P_PORTA_MODE,
    P_PORTA_TIME,
    P_CHORUS,
    P_TREMOLO,
    P_FX_SPEED,
    P_FX_DEPTH,
    P_RIBBON,
    P_BEND_RANGE,
    P_EXPRESSION,
    P_DRIFT,
    P_REV_MIX,
    P_REV_DECAY,
    P_REV_TONE,
    P_REV_PREDELAY,
    P_GAIN,           // per-preset loudness trim [A], appended 2026-10-09
    P_ENV_LONG,       // Long envelope mode [A], appended 2026-10-09
    P_OVERSAMPLE,     // 4x oversampling [A] (plan 1.10), appended 2026-10-09
    PARAM_COUNT
};

enum ParamFlags : uint32_t {
    PF_STORED = 1u << 0,     // part of a software preset (patch)
    PF_STEPPED = 1u << 1,    // integer values only
    PF_BIPOLAR = 1u << 2,    // centre is neutral
};

// How the value is shown as text.
enum class ParamUnit : uint8_t {
    Plain,        // 0..1 shown as 0..10 (the hardware's slider scale)
    Feet,
    Switch,
    Time,         // seconds, law per parameter (paramTimeLaw)
    Hertz,        // rate, law per parameter (paramRateHz)
    Percent,
    Semitones,
    SubFunc,
    SusMode,
    PortaMode,
    Bipolar,      // -1..1 shown as -10..+10
    Decibel,
    Physical,     // circuit value: Hz at C4, Q, V, %, st, dB ... (spec 04 "Value display")
};

struct ParamInfo {
    const char* key;       // stable text key, e.g. "l1.lpf"
    const char* name;      // display name, e.g. "I LPF"
    const char* module;    // CLAP module path
    double min, max, def;
    uint32_t flags;
    ParamUnit unit;
};

const ParamInfo& paramInfo(uint32_t id);
// -1 if the key is unknown.
int paramIdFromKey(const char* key);
// Display text (fills buf). longEnv: the Long envelope mode is on (it changes
// the envelope time ranges shown).
void paramValueText(uint32_t id, double value, char* buf, size_t cap, bool longEnv = false);
double clampParam(uint32_t id, double v);
// Parses a value typed in the units paramValueText shows (the inverse mapping).
bool paramTextToValue(uint32_t id, const char* text, double* out, bool longEnv = false);

// Time laws shared by the DSP and the value display (spec 02 §5/§6, 03 §8/§9):
// T = tmin * (tmax/tmin)^pos, ranges per control.
enum class TimeLaw : uint8_t {
    VcfAttack, VcfDecay, VcfRelease,
    VcaAttack, VcaDecay, VcaRelease,
    RmAttack, RmDecay,
    Sustain,
};
double timeSec(TimeLaw law, double pos, bool longEnv = false);
double timePos(TimeLaw law, double sec, bool longEnv = false);   // inverse, clamped 0..1
// The time law of a Time-unit parameter (false for other parameters).
bool paramTimeLaw(uint32_t id, TimeLaw* law);

// Rate laws (spec 02 §2, 03 §3/§4/§9): slider position -> Hz.
double pwmRateHz(double pos);      // PWM LFO 0.1 .. 127 Hz
double subRateHz(double pos);      // sub-oscillator 0.5 .. 100 Hz
double rmRateHz(double v);         // ring-mod carrier 0.25 + 204.75 Hz/unit (linear, v may exceed 1)
// Rate in Hz of a Hertz-unit parameter, and its inverse.
double paramRateHz(uint32_t id, double pos);
double paramRatePos(uint32_t id, double hz);

} // namespace tannhauser

#endif
