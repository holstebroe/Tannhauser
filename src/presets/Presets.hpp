#ifndef TANNHAUSER_PRESETS_HPP
#define TANNHAUSER_PRESETS_HPP

// Software presets (spec 05): built-in presets generated into
// PresetData.cpp by tools/gen_presets.py, plus user .tpreset files.

#include "core/Params.hpp"
#include <array>
#include <string>
#include <vector>

namespace tannhauser {

using ParamValues = std::array<double, PARAM_COUNT>;

struct PresetValue { const char* key; double value; };
struct BuiltinPreset { const char* name; const PresetValue* values; int count; };

// Generated (PresetData.cpp).
extern const BuiltinPreset kBuiltinPresets[];
extern const int kBuiltinPresetCount;
// Factory tones in tone-selector order: [channel][button 0..10] -> index into
// kBuiltinPresets of the FT preset ("FT String 1"...), and the line it is on.
extern const int kFactoryToneIndex[2][11];
extern const char* const kFactoryToneNames[2][11];

struct Preset {
    std::string name;       // "PD Blade Pad"
    std::string category;   // "PD"
    bool user = false;
    std::string path;       // user presets
    ParamValues values;     // every parameter; only PF_STORED ones are applied
};

// Defaults for every parameter (the Init patch).
ParamValues defaultValues();

// "key=value" text (one per line, '#' comments). Values not present keep
// `base`. Returns false when nothing could be parsed.
bool parsePatchText(const std::string& text, ParamValues& values, std::string* name);
// Writes the stored (S) parameters, or all of them.
std::string patchToText(const ParamValues& values, const std::string& name, bool storedOnly);

class PresetLibrary {
public:
    PresetLibrary();
    const std::vector<Preset>& presets() const { return presets_; }
    int count() const { return static_cast<int>(presets_.size()); }
    int indexOf(const std::string& name) const;
    // Category codes in menu order (spec 05 §2), only those present, plus "USER".
    std::vector<std::string> categories() const;
    static std::string categoryName(const std::string& code);
    // Re-scans the user folder.
    void scanUserPresets();
    static std::string userPresetDirectory();
    bool saveUserPreset(const std::string& path, const ParamValues& values, std::string& name, std::string& error);
private:
    std::vector<Preset> presets_;
    int builtinCount_ = 0;
};

} // namespace tannhauser

#endif
