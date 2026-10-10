#include "Params.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace tannhauser {

namespace {

struct LineTemplate {
    const char* key;
    const char* name;
    const char* module;
    double min, max, def1, def2;   // default for line I, line II
    uint32_t flags;
    ParamUnit unit;
};

constexpr uint32_t S = PF_STORED;
constexpr uint32_t T = PF_STEPPED;
constexpr uint32_t B = PF_BIPOLAR;

// Order = LineParam.
const LineTemplate kLine[LP_COUNT] = {
    { "feet",        "Feet",                     "VCO", 0, 5, 1, 1, S | T, ParamUnit::Feet },
    { "pwmSpeed",    "PWM Speed",                "VCO", 0, 1, 0.232, 0.232, S, ParamUnit::Hertz },
    { "pwmDepth",    "PWM Depth",                "VCO", 0, 1, 0, 0, S, ParamUnit::Physical },
    { "pw",          "Pulse Width",              "VCO", 0, 1, 0, 0, S, ParamUnit::Physical },
    { "square",      "Pulse Wave",               "VCO", 0, 1, 0, 0, S | T, ParamUnit::Switch },
    { "saw",         "Saw Wave",                 "VCO", 0, 1, 1, 1, S | T, ParamUnit::Switch },
    { "noise",       "Noise Level",              "VCO", 0, 1, 0, 0, S, ParamUnit::Plain },
    { "hpf",         "HPF Cutoff",               "VCF", 0, 1, 0, 0, S, ParamUnit::Physical },
    { "resH",        "HPF Resonance",            "VCF", 0, 1, 0, 0, S, ParamUnit::Physical },
    { "lpf",         "LPF Cutoff",               "VCF", 0, 1, 0.7, 0.7, S, ParamUnit::Physical },
    { "resL",        "LPF Resonance",            "VCF", 0, 1, 0.2, 0.2, S, ParamUnit::Physical },
    { "il",          "VCF Initial Level (IL)",   "VCF", 0, 1, 0, 0, S, ParamUnit::Physical },
    { "al",          "VCF Attack Level (AL)",    "VCF", 0, 1, 0.3, 0.3, S, ParamUnit::Physical },
    { "fegA",        "VCF Attack Time",          "VCF", 0, 1, 0.243, 0.243, S, ParamUnit::Time },
    { "fegD",        "VCF Decay Time",           "VCF", 0, 1, 0.604, 0.604, S, ParamUnit::Time },
    { "fegR",        "VCF Release Time",         "VCF", 0, 1, 0.508, 0.508, S, ParamUnit::Time },
    { "vcfLevel",    "VCF Level",                "VCA", 0, 1, 1, 1, S, ParamUnit::Plain },
    { "sine",        "Sine Level",               "VCA", 0, 1, 0, 0, S, ParamUnit::Plain },
    { "vegA",        "VCA Attack Time",          "VCA", 0, 1, 0.056, 0.056, S, ParamUnit::Time },
    { "vegD",        "VCA Decay Time",           "VCA", 0, 1, 0.617, 0.617, S, ParamUnit::Time },
    { "vegS",        "VCA Sustain Level",        "VCA", 0, 1, 0.8, 0.8, S, ParamUnit::Plain },
    { "vegR",        "VCA Release Time",         "VCA", 0, 1, 0.505, 0.505, S, ParamUnit::Time },
    { "level",       "Line Level",               "VCA", 0, 1, 0.8, 0.8, S, ParamUnit::Plain },
    { "initBrill",   "Initial Touch Brilliance", "Touch", 0, 1, 0.3, 0.3, S, ParamUnit::Plain },
    { "initLevel",   "Initial Touch Level",      "Touch", 0, 1, 0.5, 0.5, S, ParamUnit::Plain },
    { "afterBrill",  "After Touch Brilliance",   "Touch", 0, 1, 0.3, 0.3, S, ParamUnit::Plain },
    { "afterLevel",  "After Touch Level",        "Touch", 0, 1, 0.3, 0.3, S, ParamUnit::Plain },
};

// Order = GlobalParam.
const ParamInfo kGlobal[PARAM_COUNT - P_VOLUME] = {
    { "volume",      "Volume",                   "Main", 0, 1, 0.7, 0, ParamUnit::Physical },
    { "pitch",       "Pitch",                    "Main", -1, 1, 0, B, ParamUnit::Semitones },
    { "detune",      "Detune Line II",           "Main", 0, 1, 0, S, ParamUnit::Physical },
    { "mix",         "Mix I/II",                 "Main", 0, 1, 0.5, S, ParamUnit::Physical },
    { "brilliance",  "Brilliance",               "Main", -1, 1, 0, S | B, ParamUnit::Physical },
    { "resonance",   "Resonance",                "Main", -1, 1, 0, S | B, ParamUnit::Physical },
    { "sub.func",    "Sub Osc Function",         "Sub Oscillator", 0, 5, 0, S | T, ParamUnit::SubFunc },
    { "sub.speed",   "Sub Osc Speed",            "Sub Oscillator", 0, 1, 0.342, S, ParamUnit::Hertz },
    { "sub.vco",     "Sub Osc VCO",              "Sub Oscillator", 0, 1, 0, S, ParamUnit::Physical },
    { "sub.vcf",     "Sub Osc VCF",              "Sub Oscillator", 0, 1, 0, S, ParamUnit::Physical },
    { "sub.vca",     "Sub Osc VCA",              "Sub Oscillator", 0, 1, 0, S, ParamUnit::Physical },
    { "touch.bend",  "Initial Touch Pitch Bend", "Touch Response", 0, 1, 0, S, ParamUnit::Physical },
    { "touch.speed", "After Touch Sub Osc Speed", "Touch Response", 0, 1, 0, S, ParamUnit::Plain },
    { "touch.vco",   "After Touch Sub Osc VCO",  "Touch Response", 0, 1, 0, S, ParamUnit::Plain },
    { "touch.vcf",   "After Touch Sub Osc VCF",  "Touch Response", 0, 1, 0, S, ParamUnit::Plain },
    { "kbd.brillLow", "Kbd Brilliance Low",       "Keyboard Control", -1, 1, 0, S | B, ParamUnit::Physical },
    { "kbd.brillHigh", "Kbd Brilliance High",      "Keyboard Control", -1, 1, 0, S | B, ParamUnit::Physical },
    { "kbd.levelLow", "Kbd Level Low",            "Keyboard Control", -1, 1, 0, S | B, ParamUnit::Physical },
    { "kbd.levelHigh", "Kbd Level High",           "Keyboard Control", -1, 1, 0, S | B, ParamUnit::Physical },
    { "rm.attack",   "Ring Mod Attack",          "Ring Modulator", 0, 1, 0, S, ParamUnit::Time },
    { "rm.decay",    "Ring Mod Decay",           "Ring Modulator", 0, 1, 0.589, S, ParamUnit::Time },
    { "rm.depth",    "Ring Mod Depth",           "Ring Modulator", 0, 1, 0, S, ParamUnit::Physical },
    { "rm.speed",    "Ring Mod Speed",           "Ring Modulator", 0, 1, 0.3, S, ParamUnit::Hertz },
    { "rm.mod",      "Ring Mod Modulation",      "Ring Modulator", 0, 1, 0, S, ParamUnit::Physical },
    { "sus.mode",    "Sustain Mode",             "Performance", 0, 1, 0, S | T, ParamUnit::SusMode },
    { "sus.time",    "Sustain Time",             "Performance", 0, 1, 0.4, S, ParamUnit::Time },
    { "sus.pedal",   "Sustain Pedal",            "Performance", 0, 1, 0, T, ParamUnit::Switch },
    { "porta.mode",  "Portamento/Glissando",     "Performance", 0, 1, 0, S | T, ParamUnit::PortaMode },
    { "porta.time",  "Portamento Time",          "Performance", 0, 1, 0, S, ParamUnit::Physical },
    { "chorus",      "Chorus",                   "Effects", 0, 1, 0, S | T, ParamUnit::Switch },
    { "tremolo",     "Tremolo",                  "Effects", 0, 1, 0, S | T, ParamUnit::Switch },
    { "fx.speed",    "Chorus/Trem Speed",        "Effects", 0, 1, 0.3, S, ParamUnit::Physical },
    { "fx.depth",    "Chorus/Trem Depth",        "Effects", 0, 1, 0.5, S, ParamUnit::Plain },
    { "ribbon",      "Ribbon",                   "Performance", -1, 1, 0, B, ParamUnit::Physical },
    { "bendRange",   "Bend Range",               "Performance", 0, 12, 2, T, ParamUnit::Semitones },
    { "expression",  "Expression",               "Performance", 0, 1, 1, 0, ParamUnit::Percent },
    { "drift",       "Drift",                    "Main", 0, 1, 0.35, 0, ParamUnit::Percent },
    { "rev.mix",     "Reverb Mix",               "Reverb", 0, 1, 0, S, ParamUnit::Percent },
    { "rev.decay",   "Reverb Decay",             "Reverb", 0, 1, 0.6, S, ParamUnit::Plain },
    { "rev.tone",    "Reverb Tone",              "Reverb", 0, 1, 0.6, S, ParamUnit::Plain },
    { "rev.predelay", "Reverb Pre-delay",         "Reverb", 0, 1, 0.2, S, ParamUnit::Physical },
    { "gain",        "Patch Gain",               "Main", -24, 24, 0, S, ParamUnit::Decibel },
    { "env.long",    "Long Envelopes",           "Main", 0, 1, 0, S | T, ParamUnit::Switch },
    { "os.4x",       "4x Oversampling",          "Main", 0, 1, 0, T, ParamUnit::Switch },
    { "ribbon.touch", "Ribbon Touch",             "Performance", 0, 1, 0, T, ParamUnit::Switch },
    { "ribbon.hold", "Ribbon Hold",              "Performance", 0, 1, 1, T, ParamUnit::Switch },
};

struct Table {
    std::vector<std::string> strings;   // owns the per-line keys and names
    ParamInfo info[PARAM_COUNT];
    Table() {
        strings.reserve(2 * LP_COUNT * 3);
        for (int line = 0; line < 2; ++line) {
            for (uint32_t p = 0; p < LP_COUNT; ++p) {
                const LineTemplate& t = kLine[p];
                strings.push_back(std::string(line == 0 ? "l1." : "l2.") + t.key);
                const char* key = strings.back().c_str();
                strings.push_back(std::string(line == 0 ? "I " : "II ") + t.name);
                const char* name = strings.back().c_str();
                strings.push_back(std::string(line == 0 ? "Line I/" : "Line II/") + t.module);
                const char* module = strings.back().c_str();
                info[lineParam(line, static_cast<LineParam>(p))] =
                    { key, name, module, t.min, t.max, line == 0 ? t.def1 : t.def2, t.flags, t.unit };
            }
        }
        for (uint32_t i = P_VOLUME; i < PARAM_COUNT; ++i) info[i] = kGlobal[i - P_VOLUME];
    }
};

const Table& table() {
    static const Table t;   // strings reserved up front: c_str() pointers stay valid
    return t;
}

} // namespace

const ParamInfo& paramInfo(uint32_t id) {
    return table().info[id < PARAM_COUNT ? id : 0];
}

int paramIdFromKey(const char* key) {
    if (!key) return -1;
    for (uint32_t i = 0; i < PARAM_COUNT; ++i) {
        if (std::strcmp(table().info[i].key, key) == 0) return static_cast<int>(i);
    }
    return -1;
}

double clampParam(uint32_t id, double v) {
    const ParamInfo& p = paramInfo(id);
    if (!std::isfinite(v)) return p.def;
    if (v < p.min) v = p.min;
    if (v > p.max) v = p.max;
    if (p.flags & PF_STEPPED) v = std::round(v);
    return v;
}

namespace {
struct TimeRange { double tmin, tmax, tmaxLong; };
// Classic ranges: Arturia CS-80 V manual §5.2 (from the hardware); Long [A]:
// the same manual's Long mode (spec 02 §5/§6). Sustain time: spec 03 §8.
const TimeRange kTimeRange[] = {
    { 0.002, 0.580, 10.0 },    // VcfAttack
    { 0.002, 8.75, 25.0 },     // VcfDecay
    { 0.002, 11.0, 40.0 },     // VcfRelease
    { 0.002, 0.885, 10.0 },    // VcaAttack
    { 0.002, 7.35, 25.0 },     // VcaDecay
    { 0.002, 11.5, 40.0 },     // VcaRelease
    { 0.003, 0.530, 0.530 },   // RmAttack
    { 0.007, 4.50, 4.50 },     // RmDecay
    { 0.010, 10.0, 10.0 },     // Sustain
};
} // namespace

double timeSec(TimeLaw law, double pos, bool longEnv) {
    const TimeRange& r = kTimeRange[static_cast<int>(law)];
    return r.tmin * std::pow((longEnv ? r.tmaxLong : r.tmax) / r.tmin, pos);
}

double timePos(TimeLaw law, double sec, bool longEnv) {
    const TimeRange& r = kTimeRange[static_cast<int>(law)];
    if (!(sec > r.tmin)) return 0.0;
    const double x = std::log(sec / r.tmin) / std::log((longEnv ? r.tmaxLong : r.tmax) / r.tmin);
    return x > 1.0 ? 1.0 : x;
}

bool paramTimeLaw(uint32_t id, TimeLaw* law) {
    TimeLaw l;
    if (id < 2 * LP_COUNT) {
        switch (id % LP_COUNT) {
            case LP_FEG_A: l = TimeLaw::VcfAttack; break;
            case LP_FEG_D: l = TimeLaw::VcfDecay; break;
            case LP_FEG_R: l = TimeLaw::VcfRelease; break;
            case LP_VEG_A: l = TimeLaw::VcaAttack; break;
            case LP_VEG_D: l = TimeLaw::VcaDecay; break;
            case LP_VEG_R: l = TimeLaw::VcaRelease; break;
            default: return false;
        }
    } else if (id == P_RM_ATTACK) {
        l = TimeLaw::RmAttack;
    } else if (id == P_RM_DECAY) {
        l = TimeLaw::RmDecay;
    } else if (id == P_SUS_TIME) {
        l = TimeLaw::Sustain;
    } else {
        return false;
    }
    if (law) *law = l;
    return true;
}

double pwmRateHz(double pos) { return 0.1 * std::pow(1270.0, pos); }
double subRateHz(double pos) { return 0.5 * std::pow(200.0, pos); }
double rmRateHz(double v) { return 0.25 + 204.75 * v; }

double paramRateHz(uint32_t id, double pos) {
    if (id == P_SUB_SPEED) return subRateHz(pos);
    if (id == P_RM_SPEED) return rmRateHz(pos);
    return pwmRateHz(pos);
}

double paramRatePos(uint32_t id, double hz) {
    double x;
    if (id == P_SUB_SPEED) x = hz > 0.5 ? std::log(hz / 0.5) / std::log(200.0) : 0.0;
    else if (id == P_RM_SPEED) x = (hz - 0.25) / 204.75;
    else x = hz > 0.1 ? std::log(hz / 0.1) / std::log(1270.0) : 0.0;
    return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x);
}

namespace {

// Physical value of a Physical-unit control (spec 04 "Value display"). Key-tracked
// quantities are given at middle C (MIDI 60) at 8'.
struct Shown {
    double v;
    const char* unit;
    int decimals;
    const char* prefix;   // "+" = explicit sign, "Q ", "+/-" (plus-minus) ...
};

constexpr double kTrackC4 = 400.0;   // filter Hz per volt at C4: 261.63 / 130.81 x 200 (spec 02 §4)

bool shownValue(uint32_t id, double x, Shown& s) {
    if (id < 2 * LP_COUNT) {
        switch (id % LP_COUNT) {
            case LP_PW: s = { 50.0 + 40.0 * x, "%", 0, "" }; return true;
            case LP_PWM_DEPTH: s = { 40.0 * x, "%", 0, "+/-" }; return true;
            case LP_HPF: s = { std::max(20.0, 0.47 * 10.0 * x * kTrackC4), "Hz", 0, "" }; return true;
            case LP_LPF: s = { std::max(20.0, 10.0 * x * kTrackC4), "Hz", 0, "" }; return true;
            case LP_RES_H:
            case LP_RES_L: s = { 0.5 * std::pow(10.0, x), "", 2, "Q " }; return true;
            case LP_IL: s = { -5.0 * x, "V", 2, "" }; return true;
            case LP_AL: s = { 5.0 * x, "V", 2, "+" }; return true;
            default: return false;
        }
    }
    switch (id) {
        case P_VOLUME: s = { x > 1e-5 ? 20.0 * std::log10(1.6 * x * x) : -200.0, "dB", 1, "+" }; return true;
        case P_DETUNE: s = { 12.0 * x * x, "Hz", 2, "+" }; return true;
        case P_MIX: s = { 2.0 * x - 1.0, "", 2, "+" }; return true;          // -1 = line I, +1 = line II
        case P_BRILLIANCE: s = { 4.0 * x, "V", 2, "+" }; return true;
        case P_RESONANCE: s = { x, "", 2, "+" }; return true;
        case P_SUB_VCO: s = { 12.0 * x * x, "st", 2, "+/-" }; return true;
        case P_SUB_VCF: s = { 5.0 * x, "V", 2, "+/-" }; return true;
        case P_SUB_VCA: s = { 100.0 * x, "%", 0, "" }; return true;
        case P_TOUCH_BEND: s = { -2.0 * x, "st", 2, "" }; return true;
        case P_KBD_BRILL_LOW:
        case P_KBD_BRILL_HIGH: s = { 3.0 * x, "V", 2, "+" }; return true;
        case P_KBD_LEVEL_LOW:
        case P_KBD_LEVEL_HIGH: {
            const double g = 1.0 + 0.8 * x;
            s = { g > 1e-5 ? 20.0 * std::log10(g) : -200.0, "dB", 1, "+" };
            return true;
        }
        case P_RM_DEPTH: s = { 204.75 * x, "Hz", 0, "+" }; return true;
        case P_RM_MOD: s = { 100.0 * x, "%", 0, "" }; return true;
        case P_PORTA_TIME: s = { 1.4 * std::pow(1000.0, x), "ms/st", 0, "" }; return true;
        case P_FX_SPEED: s = { 0.2 * std::pow(40.0, x), "Hz", 2, "" }; return true;   // chorus; tremolo is 0.5 Hz * 20^x
        case P_RIBBON: s = { 12.0 * x, "st", 2, "+" }; return true;
        case P_REV_PREDELAY: s = { 150.0 * x, "ms", 0, "" }; return true;
        default: return false;
    }
}

void shownText(const Shown& s, char* buf, size_t cap) {
    if (s.v <= -199.0) { std::snprintf(buf, cap, "-inf %s", s.unit); return; }
    double v = s.v;
    if (std::fabs(v) < 0.5 * std::pow(10.0, -s.decimals)) v = 0.0;   // no "-0.00"
    const char* unit = s.unit;
    int dec = s.decimals;
    if (std::strcmp(unit, "Hz") == 0 && std::fabs(v) >= 1000.0) { v /= 1000.0; unit = "kHz"; dec = 2; }
    if (std::strcmp(unit, "ms/st") == 0 && v >= 1000.0) { v /= 1000.0; unit = "s/st"; dec = 2; }
    const bool sign = std::strcmp(s.prefix, "+") == 0;
    char num[32];
    std::snprintf(num, sizeof num, sign ? "%+.*f" : "%.*f", dec, v);
    std::snprintf(buf, cap, "%s%s%s%s", sign ? "" : s.prefix, num, *unit ? " " : "", unit);
}

} // namespace

const char* const kFeet[] = { "16'", "8'", "5 1/3'", "4'", "2 2/3'", "2'" };
const char* const kSub[] = { "Sine", "Saw Up", "Saw Down", "Square", "Sample & Hold", "Noise" };

static void timeText(double sec, char* buf, size_t cap) {
    if (sec < 1.0) std::snprintf(buf, cap, "%.0f ms", sec * 1000.0);
    else std::snprintf(buf, cap, "%.2f s", sec);
}

void paramValueText(uint32_t id, double value, char* buf, size_t cap, bool longEnv) {
    if (!buf || cap == 0) return;
    const ParamInfo& p = paramInfo(id);
    value = clampParam(id, value);
    switch (p.unit) {
        case ParamUnit::Feet: std::snprintf(buf, cap, "%s", kFeet[static_cast<int>(value)]); break;
        case ParamUnit::Switch: std::snprintf(buf, cap, "%s", value >= 0.5 ? "On" : "Off"); break;
        case ParamUnit::Time: {
            TimeLaw law = TimeLaw::Sustain;
            paramTimeLaw(id, &law);
            timeText(timeSec(law, value, longEnv), buf, cap);
            break;
        }
        case ParamUnit::Hertz: {
            const double hz = paramRateHz(id, value);
            std::snprintf(buf, cap, hz < 10.0 ? "%.2f Hz" : "%.1f Hz", hz);
            break;
        }
        case ParamUnit::Percent: std::snprintf(buf, cap, "%.0f %%", value * 100.0); break;
        case ParamUnit::Semitones: std::snprintf(buf, cap, "%+.2f st", value); break;
        case ParamUnit::SubFunc: std::snprintf(buf, cap, "%s", kSub[static_cast<int>(value)]); break;
        case ParamUnit::SusMode: std::snprintf(buf, cap, "%s", value >= 0.5 ? "II" : "I"); break;
        case ParamUnit::PortaMode: std::snprintf(buf, cap, "%s", value >= 0.5 ? "Glissando" : "Portamento"); break;
        case ParamUnit::Bipolar: std::snprintf(buf, cap, "%+.1f", value * 10.0); break;
        case ParamUnit::Physical: {
            Shown sh{};
            if (shownValue(id, value, sh)) shownText(sh, buf, cap);
            else std::snprintf(buf, cap, "%.1f", value * 10.0);
            // The cutoff follows the key (spec 02 §4): say which key the Hz are for.
            if (id < 2 * LP_COUNT && (id % LP_COUNT == LP_HPF || id % LP_COUNT == LP_LPF)) {
                const size_t n = std::strlen(buf);
                std::snprintf(buf + n, cap - n, " at C4");
            }
            break;
        }
        case ParamUnit::Decibel: std::snprintf(buf, cap, "%+.1f dB", value); break;
        case ParamUnit::Plain:
        default: std::snprintf(buf, cap, "%.1f", value * 10.0); break;
    }
    if (id == P_BEND_RANGE) std::snprintf(buf, cap, "%.0f st", value);
    if (id == P_PORTA_TIME && value <= 0.0) std::snprintf(buf, cap, "Off");
}

const char* paramDescription(uint32_t id) {
    if (id < 2 * LP_COUNT) {
        switch (id % LP_COUNT) {
            case LP_FEET: return "Footage (octave/fifth) of this line's VCO";
            case LP_PWM_SPEED: return "Rate of this line's PWM LFO (a sine)";
            case LP_PWM_DEPTH: return "How far the PWM LFO moves the pulse width";
            case LP_PW: return "Pulse width: 50 % is a square, 90 % a narrow pulse";
            case LP_SQUARE: return "Pulse wave on/off (any mix of pulse and saw)";
            case LP_SAW: return "Sawtooth wave on/off";
            case LP_NOISE: return "White noise into the filters (shared by all voices)";
            case LP_HPF: return "High-pass cutoff; follows the key, half the modulation of the LPF";
            case LP_RES_H: return "High-pass resonance (never self-oscillates)";
            case LP_LPF: return "Low-pass cutoff; follows the key. The filter envelope moves around it";
            case LP_RES_L: return "Low-pass resonance (never self-oscillates)";
            case LP_IL: return "Filter envelope start, this far below the cutoff; release returns here";
            case LP_AL: return "Filter envelope peak above the cutoff, reached at the end of the attack";
            case LP_FEG_A: return "Filter envelope: time from IL up to AL";
            case LP_FEG_D: return "Filter envelope: time from AL down to the cutoff (it sustains at 0)";
            case LP_FEG_R: return "Filter envelope: time from the cutoff back to IL after key release";
            case LP_VCF_LEVEL: return "Amount of the filtered signal fed to the VCA";
            case LP_SINE: return "Sine wave into the VCA, bypassing the filters";
            case LP_VEG_A: return "Amplitude envelope: time from 0 to the peak";
            case LP_VEG_D: return "Amplitude envelope: time from the peak down to the sustain level";
            case LP_VEG_S: return "Amplitude envelope: level held while the key is down";
            case LP_VEG_R: return "Amplitude envelope: time to silence after key release";
            case LP_LEVEL: return "Output level of this line";
            case LP_INIT_BRILL: return "Velocity (initial touch) opens the filters";
            case LP_INIT_LEVEL: return "Velocity (initial touch) raises the level";
            case LP_AFTER_BRILL: return "Key pressure (after touch) opens the filters";
            case LP_AFTER_LEVEL: return "Key pressure (after touch) raises the level";
            default: return "";
        }
    }
    switch (id) {
        case P_VOLUME: return "Master volume";
        case P_PITCH: return "Master tuning, +-1 semitone";
        case P_DETUNE: return "Raises line II by a constant number of Hz: the beat rate is the same on every key";
        case P_MIX: return "Balance between line I (up) and line II (down); both full in the middle";
        case P_BRILLIANCE: return "Cutoff offset for both filters of both lines";
        case P_RESONANCE: return "Resonance offset for both filters of both lines";
        case P_SUB_FUNC: return "Sub-oscillator (global LFO) waveform";
        case P_SUB_SPEED: return "Sub-oscillator rate";
        case P_SUB_VCO: return "Sub-oscillator to pitch (vibrato)";
        case P_SUB_VCF: return "Sub-oscillator to the filter cutoffs (wah)";
        case P_SUB_VCA: return "Sub-oscillator to the level (tremolo)";
        case P_TOUCH_BEND: return "Hard-struck notes start flat and slide up to pitch";
        case P_TOUCH_SPEED: return "Key pressure speeds up the sub-oscillator";
        case P_TOUCH_VCO: return "Key pressure adds sub-oscillator vibrato";
        case P_TOUCH_VCF: return "Key pressure adds sub-oscillator filter modulation";
        case P_KBD_BRILL_LOW: return "Cutoff offset for the lowest key, fading to none at the middle";
        case P_KBD_BRILL_HIGH: return "Cutoff offset for the highest key, fading to none at the middle";
        case P_KBD_LEVEL_LOW: return "Level offset for the lowest key, fading to none at the middle";
        case P_KBD_LEVEL_HIGH: return "Level offset for the highest key, fading to none at the middle";
        case P_RM_ATTACK: return "Ring modulator envelope attack (raises the carrier speed)";
        case P_RM_DECAY: return "Ring modulator envelope decay";
        case P_RM_DEPTH: return "How far the envelope raises the carrier speed";
        case P_RM_SPEED: return "Ring modulator carrier frequency";
        case P_RM_MOD: return "Ring modulation amount, dry to fully ring-modulated";
        case P_SUS_MODE: return "I: every released note sustains. II: only the last note or chord";
        case P_SUS_TIME: return "Release added while the sustain pedal is down";
        case P_SUS_PEDAL: return "Sustain foot switch (also MIDI CC 64)";
        case P_PORTA_MODE: return "Portamento glides smoothly; glissando steps in semitones";
        case P_PORTA_TIME: return "Glide time per semitone from the last played key; lowest = off";
        case P_CHORUS: return "BBD chorus (stereo)";
        case P_TREMOLO: return "Stereo tremolo";
        case P_FX_SPEED: return "Chorus / tremolo rate";
        case P_FX_DEPTH: return "Chorus / tremolo depth";
        case P_RIBBON: return "Ribbon pitch, relative to where it is touched (+-1 octave)";
        case P_BEND_RANGE: return "MIDI pitch-bend range";
        case P_EXPRESSION: return "Expression pedal (MIDI CC 11)";
        case P_DRIFT: return "Analog spread: per-card tuning, filter and envelope errors, drift and jitter";
        case P_REV_MIX: return "Reverb amount (added feature)";
        case P_REV_DECAY: return "Reverb decay";
        case P_REV_TONE: return "Reverb brightness";
        case P_REV_PREDELAY: return "Delay before the reverb starts";
        case P_GAIN: return "Per-preset loudness trim";
        case P_ENV_LONG: return "Long envelopes: filter/VCA times up to 10 s attack, 25 s decay, 40 s release";
        case P_OVERSAMPLE: return "4x oversampling: less aliasing from driven filters and audio-rate modulation";
        case P_RIBBON_TOUCH: return "Set while the ribbon is touched";
        case P_RIBBON_HOLD: return "Bent notes keep the ribbon pitch on release (off: they return, as on the CS-80)";
        default: return "";
    }
}

bool paramTextToValue(uint32_t id, const char* text, double* out, bool longEnv) {
    if (id >= PARAM_COUNT || !text || !out) return false;
    const ParamInfo& p = paramInfo(id);
    if (p.unit == ParamUnit::Switch) {
        *out = (std::strstr(text, "On") || std::strstr(text, "on") || std::atof(text) >= 0.5) ? 1.0 : 0.0;
        return true;
    }
    if (id == P_PORTA_TIME && (std::strstr(text, "Off") || std::strstr(text, "off"))) { *out = 0.0; return true; }
    // Named positions: the shown labels (case-insensitive prefix).
    auto named = [&](const char* const* names, int n) {
        for (int i = n - 1; i >= 0; --i) {
            const size_t len = std::strlen(names[i]);
            size_t k = 0;
            while (k < len && text[k] && std::tolower(static_cast<unsigned char>(text[k])) ==
                                         std::tolower(static_cast<unsigned char>(names[i][k]))) ++k;
            if (k == len) return i;
        }
        return -1;
    };
    static const char* const kSusModes[] = { "I", "II" };
    static const char* const kPortaModes[] = { "Portamento", "Glissando" };
    int idx = -1;
    if (p.unit == ParamUnit::Feet) idx = named(kFeet, 6);
    if (p.unit == ParamUnit::SubFunc) idx = named(kSub, 6);
    if (p.unit == ParamUnit::SusMode) idx = named(kSusModes, 2);
    if (p.unit == ParamUnit::PortaMode) idx = named(kPortaModes, 2);
    if (idx >= 0) { *out = idx; return true; }
    const char* t = text;
    if (std::strncmp(t, "+/-", 3) == 0) t += 3;
    while (*t && !(std::isdigit(static_cast<unsigned char>(*t)) || *t == '-' || *t == '+' || *t == '.')) ++t;
    if (std::strstr(t, "-inf")) { *out = p.min; return true; }
    char* end = nullptr;
    double v = std::strtod(t, &end);
    if (end == t || !std::isfinite(v)) return false;
    while (*end == ' ') ++end;
    if (*end == 'k' || *end == 'K') v *= 1000.0;   // kHz
    switch (p.unit) {
        case ParamUnit::Plain:
        case ParamUnit::Bipolar: v /= 10.0; break;   // the hardware's 0..10 scale
        case ParamUnit::Percent: v /= 100.0; break;
        case ParamUnit::Time: {
            TimeLaw law = TimeLaw::Sustain;
            paramTimeLaw(id, &law);
            v = timePos(law, std::strstr(text, "ms") ? v / 1000.0 : v, longEnv);
            break;
        }
        case ParamUnit::Hertz: v = paramRatePos(id, v); break;
        case ParamUnit::Physical: {
            // Invert the shown mapping by bisection (every mapping is monotonic).
            if (id == P_PORTA_TIME && std::strstr(text, "s/") && !std::strstr(text, "ms")) v *= 1000.0;
            Shown lo{}, hi{};
            if (!shownValue(id, p.min, lo) || !shownValue(id, p.max, hi)) break;
            const bool rising = hi.v > lo.v;
            double a = p.min, b = p.max;
            for (int i = 0; i < 60; ++i) {
                const double m = 0.5 * (a + b);
                Shown sm{};
                shownValue(id, m, sm);
                if ((sm.v < v) == rising) a = m; else b = m;
            }
            v = 0.5 * (a + b);
            break;
        }
        default: break;
    }
    *out = clampParam(id, v);
    return true;
}

} // namespace tannhauser
