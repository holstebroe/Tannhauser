#ifndef TEARWASH_TWPARAMS_HPP
#define TEARWASH_TWPARAMS_HPP

// Tearwash 225 plugin parameters (docs/tearwash/03 §5): the single source of truth for ids,
// keys, ranges and value text. Ids and keys never change; new parameters are appended.

#include <cstdint>

namespace tearwash {

enum TwParam : uint32_t {
    TW_FLAVOUR, TW_PROGRAM, TW_BASS, TW_MID, TW_XOVER, TW_TREBLE, TW_DEPTH, TW_PREDELAY, TW_DIFFUSION,
    TW_MODEENH, TW_CHORUS, TW_DECAYOPT, TW_REAR, TW_SIZE, TW_DEFINITION, TW_HFBW, TW_MIX, TW_INGAIN,
    TW_OUTGAIN, TW_CLEAN, TW_BUGFIX,
    TW_PARAM_COUNT
};

enum class TwKind { Code, Choice, Toggle, Percent, Decibel };

struct TwParamInfo {
    const char* key;
    const char* name;
    const char* module;
    double min, max, def;
    TwKind kind;
};

enum TwFlavour { FL_224 = 0, FL_224X = 1, FL_224XL = 2, FL_225 = 3, FL_COUNT = 4 };
constexpr int kMaxPrograms = 32;          // range of the program parameter
constexpr double kOutGainFloorDb = -60.0; // output gain at its minimum is −∞

const TwParamInfo& twParamInfo(uint32_t id);
uint32_t twParamByKey(const char* key);   // TW_PARAM_COUNT if unknown
double twClamp(uint32_t id, double v);

// Slider code 0…255 from the stored value v/255, and back.
inline int toCode(double v) { const long c = static_cast<long>(v * 255.0 + 0.5); return c < 0 ? 0 : (c > 255 ? 255 : static_cast<int>(c)); }
inline double fromCode(int c) { return c / 255.0; }

const char* flavourName(int f);
// The programs a flavour offers. Until the 224 (V4.4) and 224X (V8.1) networks exist, those
// flavours offer the 224XL programs (PLAN TW1.5–1.6); the 225 has no program list.
int programCount(int flavour);
const char* programName(int flavour, int index);
int clampProgram(int flavour, int index);
// True while the flavour still runs the 224XL networks in place of its own.
bool flavourProvisional(int flavour);

// Value text. coreRate: the running program's sample rate (for the predelay of linear-predelay
// programs and the crossover and treble corners).
void twValueText(uint32_t id, double v, int flavour, int program, double coreRate, char* buf, uint32_t cap);
bool twTextToValue(uint32_t id, const char* text, int flavour, double* out);

// Which parameters a flavour uses (the panel dims the rest).
bool twParamActive(uint32_t id, int flavour);

} // namespace tearwash

#endif
