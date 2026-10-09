#include "Presets.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace tannhauser {

ParamValues defaultValues() {
    ParamValues v{};
    for (uint32_t i = 0; i < PARAM_COUNT; ++i) v[i] = paramInfo(i).def;
    return v;
}

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

bool parsePatchText(const std::string& text, ParamValues& values, std::string* name) {
    std::istringstream in(text);
    std::string line;
    bool any = false;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = trim(line.substr(0, eq));
        const std::string val = trim(line.substr(eq + 1));
        if (key == "name") {
            if (name) *name = val;
            any = true;
            continue;
        }
        const int id = paramIdFromKey(key.c_str());
        if (id < 0) continue;
        char* end = nullptr;
        const double v = std::strtod(val.c_str(), &end);
        if (end == val.c_str()) continue;
        values[static_cast<uint32_t>(id)] = clampParam(static_cast<uint32_t>(id), v);
        any = true;
    }
    return any;
}

std::string patchToText(const ParamValues& values, const std::string& name, bool storedOnly) {
    std::string out = "# Tannhauser preset v1\n";
    out += "name=" + name + "\n";
    char buf[96];
    for (uint32_t i = 0; i < PARAM_COUNT; ++i) {
        const ParamInfo& p = paramInfo(i);
        if (storedOnly && !(p.flags & PF_STORED)) continue;
        std::snprintf(buf, sizeof(buf), "%s=%.6g\n", p.key, values[i]);
        out += buf;
    }
    return out;
}

static const char* kCategoryOrder[] = { "IN", "FT", "FC", "PD", "ST", "BR", "LD", "BS", "KY", "OR", "PL", "BL", "SQ", "FX" };

std::string PresetLibrary::categoryName(const std::string& code) {
    static const struct { const char* code; const char* name; } kNames[] = {
        { "IN", "INIT" }, { "FT", "FACTORY TONES" }, { "FC", "FACTORY COMBOS" }, { "PD", "PADS" },
        { "ST", "STRINGS" }, { "BR", "BRASS" }, { "LD", "LEADS" }, { "BS", "BASS" }, { "KY", "KEYS" },
        { "OR", "ORGAN" }, { "PL", "PLUCK" }, { "BL", "BELLS" }, { "SQ", "SEQUENCE" }, { "FX", "EFFECTS" },
        { "USER", "USER" },
    };
    for (const auto& n : kNames) if (code == n.code) return n.name;
    return code;
}

PresetLibrary::PresetLibrary() {
    const ParamValues defaults = defaultValues();
    for (int i = 0; i < kBuiltinPresetCount; ++i) {
        const BuiltinPreset& b = kBuiltinPresets[i];
        Preset p;
        p.name = b.name;
        p.category = p.name.size() >= 2 ? p.name.substr(0, 2) : "IN";
        p.values = defaults;
        for (int k = 0; k < b.count; ++k) {
            const int id = paramIdFromKey(b.values[k].key);
            if (id >= 0) p.values[static_cast<uint32_t>(id)] = clampParam(static_cast<uint32_t>(id), b.values[k].value);
        }
        presets_.push_back(std::move(p));
    }
    builtinCount_ = static_cast<int>(presets_.size());
    scanUserPresets();
}

int PresetLibrary::indexOf(const std::string& name) const {
    for (size_t i = 0; i < presets_.size(); ++i) if (presets_[i].name == name) return static_cast<int>(i);
    return -1;
}

std::vector<std::string> PresetLibrary::categories() const {
    std::vector<std::string> out;
    for (const char* c : kCategoryOrder) {
        for (const auto& p : presets_) {
            if (!p.user && p.category == c) { out.push_back(c); break; }
        }
    }
    for (const auto& p : presets_) if (p.user) { out.push_back("USER"); break; }
    return out;
}

std::string PresetLibrary::userPresetDirectory() {
#if defined(_WIN32)
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    if (!home || !*home) return std::string();
    return (std::filesystem::u8path(home) / "Documents" / "Tannhauser" / "Presets").u8string();
}

void PresetLibrary::scanUserPresets() {
    presets_.resize(static_cast<size_t>(builtinCount_));
    const std::string dir = userPresetDirectory();
    if (dir.empty()) return;
    std::error_code ec;
    std::vector<std::filesystem::path> files;
    for (const auto& e : std::filesystem::directory_iterator(std::filesystem::u8path(dir), ec)) {
        if (e.is_regular_file(ec) && e.path().extension() == ".tpreset") files.push_back(e.path());
        if (files.size() > 2000) break;
    }
    std::sort(files.begin(), files.end());
    for (const auto& f : files) {
        std::ifstream in(f, std::ios::binary);
        if (!in) continue;
        std::stringstream ss;
        ss << in.rdbuf();
        Preset p;
        p.values = defaultValues();
        p.name = f.stem().u8string();
        std::string name;
        if (!parsePatchText(ss.str(), p.values, &name)) continue;
        if (!name.empty()) p.name = name;
        p.category = "USER";
        p.user = true;
        p.path = f.u8string();
        presets_.push_back(std::move(p));
    }
}

bool PresetLibrary::saveUserPreset(const std::string& path, const ParamValues& values, std::string& name, std::string& error) {
    std::filesystem::path fp = std::filesystem::u8path(path);
    if (fp.extension() != ".tpreset") fp += ".tpreset";
    std::error_code ec;
    std::filesystem::create_directories(fp.parent_path(), ec);
    if (name.empty()) name = fp.stem().u8string();
    std::ofstream out(fp, std::ios::binary | std::ios::trunc);
    if (!out) { error = "cannot write " + fp.u8string(); return false; }
    out << patchToText(values, name, true);
    out.close();
    if (!out) { error = "cannot write " + fp.u8string(); return false; }
    scanUserPresets();
    return true;
}

} // namespace tannhauser
