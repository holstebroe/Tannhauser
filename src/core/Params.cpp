#include "Params.hpp"
#include <cmath>
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
    { "feet",       "Feet",             "VCO", 0, 5, 1, 1, S | T, ParamUnit::Feet },
    { "pwmSpeed",   "PWM Speed",        "VCO", 0, 1, 0.232, 0.232, S, ParamUnit::Hertz },
    { "pwmDepth",   "PWM",              "VCO", 0, 1, 0, 0, S, ParamUnit::Plain },
    { "pw",         "PW",               "VCO", 0, 1, 0, 0, S, ParamUnit::Plain },
    { "square",     "Square",           "VCO", 0, 1, 0, 0, S | T, ParamUnit::Switch },
    { "saw",        "Saw",              "VCO", 0, 1, 1, 1, S | T, ParamUnit::Switch },
    { "noise",      "Noise",            "VCO", 0, 1, 0, 0, S, ParamUnit::Plain },
    { "hpf",        "HPF",              "VCF", 0, 1, 0, 0, S, ParamUnit::Plain },
    { "resH",       "Res H",            "VCF", 0, 1, 0, 0, S, ParamUnit::Plain },
    { "lpf",        "LPF",              "VCF", 0, 1, 0.7, 0.7, S, ParamUnit::Plain },
    { "resL",       "Res L",            "VCF", 0, 1, 0.2, 0.2, S, ParamUnit::Plain },
    { "il",         "IL",               "VCF", 0, 1, 0, 0, S, ParamUnit::Plain },
    { "al",         "AL",               "VCF", 0, 1, 0.3, 0.3, S, ParamUnit::Plain },
    { "fegA",       "VCF Attack",       "VCF", 0, 1, 0.243, 0.243, S, ParamUnit::Time },
    { "fegD",       "VCF Decay",        "VCF", 0, 1, 0.604, 0.604, S, ParamUnit::Time },
    { "fegR",       "VCF Release",      "VCF", 0, 1, 0.508, 0.508, S, ParamUnit::Time },
    { "vcfLevel",   "VCF Level",        "VCA", 0, 1, 1, 1, S, ParamUnit::Plain },
    { "sine",       "Sine",             "VCA", 0, 1, 0, 0, S, ParamUnit::Plain },
    { "vegA",       "VCA Attack",       "VCA", 0, 1, 0.056, 0.056, S, ParamUnit::Time },
    { "vegD",       "VCA Decay",        "VCA", 0, 1, 0.617, 0.617, S, ParamUnit::Time },
    { "vegS",       "VCA Sustain",      "VCA", 0, 1, 0.8, 0.8, S, ParamUnit::Plain },
    { "vegR",       "VCA Release",      "VCA", 0, 1, 0.505, 0.505, S, ParamUnit::Time },
    { "level",      "Level",            "VCA", 0, 1, 0.8, 0.8, S, ParamUnit::Plain },
    { "initBrill",  "Init Brilliance",  "Touch", 0, 1, 0.3, 0.3, S, ParamUnit::Plain },
    { "initLevel",  "Init Level",       "Touch", 0, 1, 0.5, 0.5, S, ParamUnit::Plain },
    { "afterBrill", "After Brilliance", "Touch", 0, 1, 0.3, 0.3, S, ParamUnit::Plain },
    { "afterLevel", "After Level",      "Touch", 0, 1, 0.3, 0.3, S, ParamUnit::Plain },
};

// Order = GlobalParam.
const ParamInfo kGlobal[PARAM_COUNT - P_VOLUME] = {
    { "volume",        "Volume",              "Main", 0, 1, 0.7, 0, ParamUnit::Plain },
    { "pitch",         "Pitch",               "Main", -1, 1, 0, B, ParamUnit::Semitones },
    { "detune",        "Detune",              "Main", 0, 1, 0, S, ParamUnit::Plain },
    { "mix",           "Mix",                 "Main", 0, 1, 0.5, S, ParamUnit::Plain },
    { "brilliance",    "Brilliance",          "Main", -1, 1, 0, S | B, ParamUnit::Bipolar },
    { "resonance",     "Resonance",           "Main", -1, 1, 0, S | B, ParamUnit::Bipolar },
    { "sub.func",      "Sub Osc Function",    "Sub Oscillator", 0, 5, 0, S | T, ParamUnit::SubFunc },
    { "sub.speed",     "Sub Osc Speed",       "Sub Oscillator", 0, 1, 0.342, S, ParamUnit::Hertz },
    { "sub.vco",       "Sub Osc VCO",         "Sub Oscillator", 0, 1, 0, S, ParamUnit::Plain },
    { "sub.vcf",       "Sub Osc VCF",         "Sub Oscillator", 0, 1, 0, S, ParamUnit::Plain },
    { "sub.vca",       "Sub Osc VCA",         "Sub Oscillator", 0, 1, 0, S, ParamUnit::Plain },
    { "touch.bend",    "Touch Pitch Bend",    "Touch Response", 0, 1, 0, S, ParamUnit::Plain },
    { "touch.speed",   "Touch Sub Speed",     "Touch Response", 0, 1, 0, S, ParamUnit::Plain },
    { "touch.vco",     "Touch Sub VCO",       "Touch Response", 0, 1, 0, S, ParamUnit::Plain },
    { "touch.vcf",     "Touch Sub VCF",       "Touch Response", 0, 1, 0, S, ParamUnit::Plain },
    { "kbd.brillLow",  "Kbd Brilliance Low",  "Keyboard Control", -1, 1, 0, S | B, ParamUnit::Bipolar },
    { "kbd.brillHigh", "Kbd Brilliance High", "Keyboard Control", -1, 1, 0, S | B, ParamUnit::Bipolar },
    { "kbd.levelLow",  "Kbd Level Low",       "Keyboard Control", -1, 1, 0, S | B, ParamUnit::Bipolar },
    { "kbd.levelHigh", "Kbd Level High",      "Keyboard Control", -1, 1, 0, S | B, ParamUnit::Bipolar },
    { "rm.attack",     "Ring Mod Attack",     "Ring Modulator", 0, 1, 0, S, ParamUnit::Time },
    { "rm.decay",      "Ring Mod Decay",      "Ring Modulator", 0, 1, 0.589, S, ParamUnit::Time },
    { "rm.depth",      "Ring Mod Depth",      "Ring Modulator", 0, 1, 0, S, ParamUnit::Plain },
    { "rm.speed",      "Ring Mod Speed",      "Ring Modulator", 0, 1, 0.3, S, ParamUnit::Hertz },
    { "rm.mod",        "Ring Mod Modulation", "Ring Modulator", 0, 1, 0, S, ParamUnit::Plain },
    { "sus.mode",      "Sustain Mode",        "Performance", 0, 1, 0, S | T, ParamUnit::SusMode },
    { "sus.time",      "Sustain Time",        "Performance", 0, 1, 0.4, S, ParamUnit::Time },
    { "sus.pedal",     "Sustain",             "Performance", 0, 1, 0, T, ParamUnit::Switch },
    { "porta.mode",    "Porta/Gliss",         "Performance", 0, 1, 0, S | T, ParamUnit::PortaMode },
    { "porta.time",    "Porta Time",          "Performance", 0, 1, 0, S, ParamUnit::Plain },
    { "chorus",        "Chorus",              "Effects", 0, 1, 0, S | T, ParamUnit::Switch },
    { "tremolo",       "Tremolo",             "Effects", 0, 1, 0, S | T, ParamUnit::Switch },
    { "fx.speed",      "Chorus/Trem Speed",   "Effects", 0, 1, 0.3, S, ParamUnit::Plain },
    { "fx.depth",      "Chorus/Trem Depth",   "Effects", 0, 1, 0.5, S, ParamUnit::Plain },
    { "ribbon",        "Ribbon",              "Performance", -1, 1, 0, B, ParamUnit::Bipolar },
    { "bendRange",     "Bend Range",          "Performance", 0, 12, 2, T, ParamUnit::Semitones },
    { "expression",    "Expression",          "Performance", 0, 1, 1, 0, ParamUnit::Percent },
    { "drift",         "Drift",               "Main", 0, 1, 0.35, 0, ParamUnit::Percent },
    { "rev.mix",       "Reverb Mix",          "Reverb", 0, 1, 0, S, ParamUnit::Percent },
    { "rev.decay",     "Reverb Decay",        "Reverb", 0, 1, 0.6, S, ParamUnit::Plain },
    { "rev.tone",      "Reverb Tone",         "Reverb", 0, 1, 0.6, S, ParamUnit::Plain },
    { "rev.predelay",  "Reverb Pre-delay",    "Reverb", 0, 1, 0.2, S, ParamUnit::Plain },
    { "gain",          "Patch Gain",          "Main", -24, 24, 0, S, ParamUnit::Decibel },
    { "env.long",      "Long Envelopes",      "Main", 0, 1, 0, S | T, ParamUnit::Switch },
    { "os.4x",         "4x Oversampling",     "Main", 0, 1, 0, T, ParamUnit::Switch },
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

static void timeText(double sec, char* buf, size_t cap) {
    if (sec < 1.0) std::snprintf(buf, cap, "%.0f ms", sec * 1000.0);
    else std::snprintf(buf, cap, "%.2f s", sec);
}

void paramValueText(uint32_t id, double value, char* buf, size_t cap, bool longEnv) {
    if (!buf || cap == 0) return;
    const ParamInfo& p = paramInfo(id);
    value = clampParam(id, value);
    static const char* kFeet[] = { "16'", "8'", "5 1/3'", "4'", "2 2/3'", "2'" };
    static const char* kSub[] = { "Sine", "Saw Up", "Saw Down", "Square", "Sample & Hold", "Noise" };
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
        case ParamUnit::Decibel: std::snprintf(buf, cap, "%+.1f dB", value); break;
        case ParamUnit::Plain:
        default: std::snprintf(buf, cap, "%.1f", value * 10.0); break;
    }
    if (id == P_BEND_RANGE) std::snprintf(buf, cap, "%.0f st", value);
    if (id == P_PORTA_TIME && value <= 0.0) std::snprintf(buf, cap, "Off");
}

} // namespace tannhauser
