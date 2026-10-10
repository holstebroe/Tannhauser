#include "TwParams.hpp"
#include "tearwash/engine/Programs.hpp"
#include "tearwash/engine/Control.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace tearwash {

namespace {

TwParamInfo g_info[TW_PARAM_COUNT] = {
    { "flavour",    "Flavour",                 "Model",   0, FL_COUNT - 1, FL_224XL, TwKind::Choice },
    { "program",    "Program",                 "Model",   0, kMaxPrograms - 1, 0, TwKind::Choice },
    { "bass",       "Bass Decay",              "Decay",   0, 1, 0, TwKind::Code },
    { "mid",        "Mid Decay",               "Decay",   0, 1, 0, TwKind::Code },
    { "xover",      "Crossover",               "Decay",   0, 1, 0, TwKind::Code },
    { "treble",     "Treble Decay",            "Decay",   0, 1, 0, TwKind::Code },
    { "depth",      "Depth",                   "Space",   0, 1, 0, TwKind::Code },
    { "predelay",   "Predelay",                "Space",   0, 1, 0, TwKind::Code },
    { "diffusion",  "Diffusion",               "Space",   0, 1, 0, TwKind::Code },
    { "modeenh",    "Mode Enhancement",        "Options", 0, 1, 1, TwKind::Toggle },
    { "chorus",     "Chorus",                  "Options", 0, 1, 0, TwKind::Code },
    { "decayopt",   "Decay Optimisation",      "Options", 0, 1, 1, TwKind::Toggle },
    { "rear",       "Rear Outputs",            "Options", 0, 1, 0, TwKind::Toggle },
    { "size",       "Size",                    "Space",   0, 1, 0, TwKind::Code },
    { "definition", "Definition",              "Space",   0, 1, 0, TwKind::Code },
    { "hfbw",       "HF Bandwidth",            "Decay",   0, 1, 0, TwKind::Code },
    { "mix",        "Mix",                     "Output",  0, 1, 0.35, TwKind::Percent },
    { "ingain",     "Input Gain",              "Output",  -12, 12, 0, TwKind::Decibel },
    { "outgain",    "Output Gain",             "Output",  kOutGainFloorDb, 12, 0, TwKind::Decibel },
    { "clean",      "Clean Converters",        "Options", 0, 1, 0, TwKind::Toggle },
    { "bugfix",     "Bug Fix",                 "Options", 0, 1, 0, TwKind::Toggle },
};

// Code defaults: the CONCERT HALL factory registers (the default program).
bool initDefaults() {
    int n = 0;
    const XlRegs r = xlFactory(xlPrograms(n)[0]);
    const struct { uint32_t id; uint8_t v; } map[] = {
        { TW_BASS, r.at(1, 1) }, { TW_MID, r.at(1, 2) }, { TW_XOVER, r.at(1, 3) }, { TW_TREBLE, r.at(1, 4) },
        { TW_DEPTH, r.at(1, 5) }, { TW_PREDELAY, r.at(1, 6) }, { TW_CHORUS, r.at(3, 3) }, { TW_HFBW, r.at(3, 4) },
        { TW_DIFFUSION, r.at(3, 5) }, { TW_DEFINITION, r.at(3, 6) }, { TW_SIZE, r.size },
    };
    for (const auto& m : map) g_info[m.id].def = fromCode(m.v);
    g_info[TW_MODEENH].def = (r.options & 0x40) ? 1.0 : 0.0;
    g_info[TW_DECAYOPT].def = (r.options & 0x80) ? 1.0 : 0.0;
    return true;
}

const char* const kFlavourNames[FL_COUNT] = { "224", "224X", "224XL", "225" };

// One-pole corner of the pair (m, 32 − m) at the core rate: f = −fs·ln(1 − m/32)/2π.
double cornerHz(int m, double fs) {
    if (m >= 32) return fs / 2.0;
    return -fs * std::log(1.0 - m / 32.0) / (2.0 * 3.14159265358979323846);
}

} // namespace

const TwParamInfo& twParamInfo(uint32_t id) {
    static const bool init = initDefaults();
    (void)init;
    return g_info[id < TW_PARAM_COUNT ? id : 0];
}

uint32_t twParamByKey(const char* key) {
    if (!key) return TW_PARAM_COUNT;
    for (uint32_t i = 0; i < TW_PARAM_COUNT; ++i)
        if (std::strcmp(twParamInfo(i).key, key) == 0) return i;
    return TW_PARAM_COUNT;
}

double twClamp(uint32_t id, double v) {
    const TwParamInfo& p = twParamInfo(id);
    if (!(v == v)) v = p.def;
    if (v < p.min) v = p.min;
    if (v > p.max) v = p.max;
    if (p.kind == TwKind::Choice || p.kind == TwKind::Toggle) v = std::floor(v + 0.5);
    return v;
}

const char* flavourName(int f) { return kFlavourNames[f < 0 ? 0 : (f >= FL_COUNT ? FL_COUNT - 1 : f)]; }

int programCount(int flavour) {
    if (flavour == FL_225) return 1;
    int n = 0;
    xlPrograms(n);
    return n;
}

const char* programName(int flavour, int index) {
    if (flavour == FL_225) return "PLATE 225";
    int n = 0;
    const ProgramInfo* p = xlPrograms(n);
    return p[clampProgram(flavour, index)].name;
}

int clampProgram(int flavour, int index) {
    const int n = programCount(flavour);
    return index < 0 ? 0 : (index >= n ? n - 1 : index);
}

bool flavourProvisional(int flavour) { return flavour == FL_224 || flavour == FL_224X; }

bool twParamActive(uint32_t id, int flavour) {
    if (flavour != FL_225) {
        if (id == TW_BUGFIX) return flavour == FL_224;
        if (id == TW_SIZE || id == TW_DEFINITION) return flavour == FL_224XL || flavourProvisional(flavour);
        return true;
    }
    switch (id) {
        case TW_FLAVOUR: case TW_MID: case TW_TREBLE: case TW_PREDELAY: case TW_MIX: case TW_INGAIN: case TW_OUTGAIN:
            return true;
        default: return false;
    }
}

void twValueText(uint32_t id, double v, int flavour, int program, double coreRate, char* buf, uint32_t cap) {
    if (!buf || cap == 0) return;
    const TwParamInfo& p = twParamInfo(id);
    v = twClamp(id, v);
    const int c = toCode(v);
    switch (id) {
        case TW_FLAVOUR: std::snprintf(buf, cap, "%s", flavourName(static_cast<int>(v))); return;
        case TW_PROGRAM: std::snprintf(buf, cap, "%s", programName(flavour, static_cast<int>(v))); return;
        case TW_PREDELAY:
            if (flavour == FL_225) break;
            {
                int n = 0;
                // CHAMBER's predelay is linear, 34 core samples per step (03 §3).
                const bool linear = xlPrograms(n)[clampProgram(flavour, program)].algorithm == 2;
                const double ms = linear ? 34.0 * c * 1000.0 / coreRate : law::predelayMs(static_cast<uint8_t>(c));
                std::snprintf(buf, cap, "%02X  %.0f ms", c, ms);
            }
            return;
        default: break;
    }
    if (p.kind == TwKind::Toggle) { std::snprintf(buf, cap, "%s", v >= 0.5 ? "On" : "Off"); return; }
    if (p.kind == TwKind::Percent) { std::snprintf(buf, cap, "%.0f %%", v * 100.0); return; }
    if (p.kind == TwKind::Decibel) {
        if (id == TW_OUTGAIN && v <= kOutGainFloorDb) std::snprintf(buf, cap, "-inf dB");
        else std::snprintf(buf, cap, "%+.1f dB", v);
        return;
    }
    if (flavour == FL_225) {   // the Tannhäuser plate's controls (CS-80 spec 03 §12)
        if (id == TW_PREDELAY) std::snprintf(buf, cap, "%.0f ms", v * 150.0);
        else std::snprintf(buf, cap, "%.0f %%", v * 100.0);
        return;
    }
    switch (id) {
        case TW_XOVER: {
            if (c == 0xFF) { std::snprintf(buf, cap, "FF  off"); return; }
            const int s = law::step5(static_cast<uint8_t>(c < 8 ? 8 : c));
            std::snprintf(buf, cap, "%02X  %.0f Hz", c, cornerHz(s, coreRate));
            return;
        }
        case TW_TREBLE: case TW_HFBW: {
            int m, rest;
            law::pair(static_cast<uint8_t>(c), m, rest);
            if (c == 0xFF) std::snprintf(buf, cap, "FF  off");
            else if (m >= 32) std::snprintf(buf, cap, "%02X  full", c);
            else std::snprintf(buf, cap, "%02X  %.0f Hz", c, cornerHz(m, coreRate));
            return;
        }
        default: std::snprintf(buf, cap, "%02X", c); return;
    }
}

bool twTextToValue(uint32_t id, const char* text, int flavour, double* out) {
    if (id >= TW_PARAM_COUNT || !text || !out) return false;
    const TwParamInfo& p = twParamInfo(id);
    if (id == TW_FLAVOUR) {
        for (int f = FL_COUNT - 1; f >= 0; --f)
            if (std::strcmp(text, kFlavourNames[f]) == 0) { *out = f; return true; }
    }
    if (id == TW_PROGRAM) {
        for (int i = 0; i < programCount(flavour); ++i)
            if (std::strcmp(text, programName(flavour, i)) == 0) { *out = i; return true; }
    }
    if (p.kind == TwKind::Toggle) {
        if (std::strcmp(text, "On") == 0 || std::strcmp(text, "on") == 0) { *out = 1; return true; }
        if (std::strcmp(text, "Off") == 0 || std::strcmp(text, "off") == 0) { *out = 0; return true; }
    }
    char* end = nullptr;
    if (p.kind == TwKind::Code && flavour != FL_225) {
        const long c = std::strtol(text, &end, 16);   // the code as the panel shows it
        if (end == text) return false;
        *out = twClamp(id, fromCode(static_cast<int>(c)));
        return true;
    }
    const double d = std::strtod(text, &end);
    if (end == text) return false;
    if (p.kind == TwKind::Percent || (p.kind == TwKind::Code && flavour == FL_225 && id != TW_PREDELAY)) *out = twClamp(id, d / 100.0);
    else if (p.kind == TwKind::Code && flavour == FL_225) *out = twClamp(id, d / 150.0);
    else *out = twClamp(id, d);
    return true;
}

} // namespace tearwash
