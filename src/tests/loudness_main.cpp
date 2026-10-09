// Preset loudness report and gain fit (spec 05 §4).
//
//   tannhauser_loudness                 table: preset, loudness (LUFS), Patch Gain
//   tannhauser_loudness --fit FILE      fit each preset's Patch Gain to the
//                                       target and write {name: dB} JSON
//
// Workflow: tannhauser_loudness --fit tools/preset_gains.json, then
// python3 tools/gen_presets.py (it applies the gains), rebuild, and check
// with tannhauser_dsp_test (T16).

#include "Loudness.hpp"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

using namespace tannhauser;

int main(int argc, char** argv) {
    PresetLibrary lib;
    const auto& presets = lib.presets();
    const bool fit = argc >= 3 && std::strcmp(argv[1], "--fit") == 0;
    std::vector<double> loud(presets.size()), gain(presets.size());
    std::atomic<size_t> next{ 0 };
    auto work = [&] {
        for (size_t i; (i = next++) < presets.size();) {
            const ParamValues& v = presets[i].values;
            double g = v[P_GAIN];
            double L = presetLoudness(v, g);
            if (fit) {
                // Gain is applied before the output soft clip, so iterate.
                for (int it = 0; it < 4 && std::fabs(L - kLoudnessTarget) > 0.1; ++it) {
                    g = std::min(24.0, std::max(-24.0, g + (kLoudnessTarget - L)));
                    L = presetLoudness(v, g);
                }
            }
            loud[i] = L;
            gain[i] = g;
        }
    };
    std::vector<std::thread> pool;
    const unsigned n = std::max(1u, std::thread::hardware_concurrency());
    for (unsigned t = 0; t < n; ++t) pool.emplace_back(work);
    for (auto& t : pool) t.join();

    for (size_t i = 0; i < presets.size(); ++i) {
        std::printf("%-28s %7.2f LUFS  gain %+6.2f dB\n", presets[i].name.c_str(), loud[i], gain[i]);
    }
    if (fit) {
        std::ofstream out(argv[2]);
        out << "{\n";
        for (size_t i = 0; i < presets.size(); ++i) {
            char buf[160];
            std::snprintf(buf, sizeof(buf), "  \"%s\": %.1f%s\n", presets[i].name.c_str(), gain[i],
                          i + 1 < presets.size() ? "," : "");
            out << buf;
        }
        out << "}\n";
        std::printf("wrote %s (target %.1f LUFS)\n", argv[2], kLoudnessTarget);
    }
    return 0;
}
