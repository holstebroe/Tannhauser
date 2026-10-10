// tearwash_oracle: render the Tearwash calibration set through the original Lexicon 224XL V8.21
// firmware, emulated by BlueBox (github.com/jimbattin/bluebox), as reference audio for
// tearwash_calib (docs/tearwash/04_VALIDATION.md).
//
// Reference-only tool: it links BlueBox's lexcore from a local checkout and reads the user's own
// ROM images. Neither BlueBox code nor ROM data is part of this repository or of any plugin.
//
// usage: tearwash_oracle --rom-dir DIR --set FILE --out DIR [--rate HZ] [--only ID]
//        tearwash_oracle --rom-dir DIR --list
//
// Set file: one case per line, '#' comments:
//   ID  PROGRAM  STIMULUS  SECONDS  [P.S=HH ...]  [opt=MASK:BITS]
// PROGRAM uses '_' for spaces (CONCERT_HALL). P.S=HH moves LARC page P (1-12) slider S (1-6)
// to hex position HH through the firmware's own handler. opt forces option bits at load
// (01 DYN DECAY, 40 MODE ENH, 80 DECAY OPT).
// Output: DIR/ID.wav (float, 4 channels = DAC A, B, C, D) and DIR/index.tsv.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "engine.h"
#include "machine.h"
#include "resampler.h"
#include "romfile.h"

#include "tearwash/analysis/Stimuli.hpp"
#include "tearwash/analysis/Wav.hpp"

using namespace tearwash;

namespace {

struct Move { int page, slider; unsigned pos; };

struct Case {
    std::string id, program, stimulus, settings;
    double seconds = 8.0;
    std::vector<Move> moves;
    unsigned optMask = 0, optBits = 0;
};

bool parseCase(const std::string& line, Case& c, std::string& err) {
    std::istringstream is(line);
    if (!(is >> c.id >> c.program >> c.stimulus >> c.seconds)) { err = "short line"; return false; }
    for (char& ch : c.program) if (ch == '_') ch = ' ';
    std::string tok;
    while (is >> tok) {
        if (!c.settings.empty()) c.settings += ' ';
        c.settings += tok;
        if (tok.rfind("opt=", 0) == 0) {
            if (std::sscanf(tok.c_str() + 4, "%x:%x", &c.optMask, &c.optBits) != 2) { err = "bad " + tok; return false; }
        } else {
            Move m{};
            if (std::sscanf(tok.c_str(), "%d.%d=%x", &m.page, &m.slider, &m.pos) != 3 || m.page < 1 || m.page > 12
                || m.slider < 1 || m.slider > 6 || m.pos > 255) { err = "bad " + tok; return false; }
            c.moves.push_back(m);
        }
    }
    return true;
}

std::string upper(std::string s) { for (char& ch : s) ch = char(std::toupper(static_cast<unsigned char>(ch))); return s; }

std::string sliderHex(const uint8_t (&v)[12][6]) {
    std::string s;
    char b[4];
    for (int p = 0; p < 12; ++p) {
        if (p) s += '|';
        for (int k = 0; k < 6; ++k) { std::snprintf(b, sizeof b, "%02X", v[p][k]); s += b; }
    }
    return s;
}

}  // namespace

int main(int argc, char** argv) {
    std::string romDir, setFile, outDir, only;
    unsigned rate = 48000;
    bool list = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--rom-dir" && i + 1 < argc) romDir = argv[++i];
        else if (a == "--set" && i + 1 < argc) setFile = argv[++i];
        else if (a == "--out" && i + 1 < argc) outDir = argv[++i];
        else if (a == "--rate" && i + 1 < argc) rate = static_cast<unsigned>(std::atoi(argv[++i]));
        else if (a == "--only" && i + 1 < argc) only = argv[++i];
        else if (a == "--list") list = true;
        else { std::fprintf(stderr, "unknown argument %s\n", a.c_str()); return 2; }
    }
    if (romDir.empty() || (!list && (setFile.empty() || outDir.empty()))) {
        std::fprintf(stderr, "usage: tearwash_oracle --rom-dir DIR --set FILE --out DIR [--rate HZ] [--only ID]\n"
                             "       tearwash_oracle --rom-dir DIR --list\n");
        return 2;
    }

    static uint8_t sbc[0x1800], nvs[0x8000];
    std::string err;
    if (!loadRomFiles(romDir, sbc, nvs, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    const RomImage roms{ sbc, nvs };
    ProgramList progs;
    listPrograms(roms, progs);
    if (list) {
        for (int i = 0; i < progs.count; ++i) std::printf("%02X %s\n", progs.items[i].id, progs.items[i].name);
        return 0;
    }

    std::ifstream in(setFile);
    if (!in) { std::fprintf(stderr, "cannot read %s\n", setFile.c_str()); return 1; }
    std::vector<Case> cases;
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line.resize(hash);
        if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
        Case c;
        if (!parseCase(line, c, err)) { std::fprintf(stderr, "%s:%d: %s\n", setFile.c_str(), lineNo, err.c_str()); return 2; }
        if (only.empty() || c.id == only) cases.push_back(c);
    }

    static float kernel[kKernelTableSize];
    static int16_t dmem[65536];
    static Engine<2, 4> engine;
    initKernelTable(kernel);

    const std::string indexPath = outDir + "/index.tsv";
    FILE* index = std::fopen(indexPath.c_str(), only.empty() ? "w" : "a");
    if (!index) { std::fprintf(stderr, "cannot write %s\n", indexPath.c_str()); return 1; }
    if (only.empty())
        std::fprintf(index, "id\tprogram\tprogram_id\tstimulus\tseconds\trate\tloop\tdsp_rate\tsettings\tsliders\toptions\n");

    for (const Case& c : cases) {
        const Program* prog = nullptr;
        for (int i = 0; i < progs.count && !prog; ++i)
            if (upper(progs.items[i].name) == upper(c.program)) prog = &progs.items[i];
        if (!prog) { std::fprintf(stderr, "%s: unknown program %s\n", c.id.c_str(), c.program.c_str()); return 2; }
        Stimulus stim = makeStimulus(c.stimulus, rate, c.seconds);
        if (!stim.valid) { std::fprintf(stderr, "%s: unknown stimulus %s\n", c.id.c_str(), c.stimulus.c_str()); return 2; }

        EngineConfig cfg{};
        cfg.roms = roms;
        cfg.dmem = dmem;
        cfg.kernel = kernel;
        cfg.sampleRate = rate;
        cfg.programId = prog->id;
        cfg.numInputs = 2;
        cfg.numOutputs = 4;
        cfg.outMix[0] = 1; cfg.outMix[1] = 2; cfg.outMix[2] = 4; cfg.outMix[3] = 8;
        cfg.optionMask = static_cast<uint8_t>(c.optMask);
        cfg.optionBits = static_cast<uint8_t>(c.optBits);
        const char* engErr = nullptr;
        if (!engine.start(cfg, &engErr)) { std::fprintf(stderr, "%s: %s\n", c.id.c_str(), engErr); return 1; }
        for (const Move& m : c.moves)
            if (!engine.moveSlider(m.page, m.slider - 1, static_cast<uint8_t>(m.pos))) {
                std::fprintf(stderr, "%s: slider %d.%d not available in %s\n", c.id.c_str(), m.page, m.slider, prog->name);
                return 2;
            }

        constexpr size_t kBlock = 48;
        float inBuf[2][kBlock] = {}, outBuf[4][kBlock];
        const float* inPtr[2] = { inBuf[0], inBuf[1] };
        float* outPtr[4] = { outBuf[0], outBuf[1], outBuf[2], outBuf[3] };
        // Settle: the firmware applies slider moves in its main loop and ramps predelay changes.
        const size_t settle = static_cast<size_t>(2.0 * rate);
        for (size_t done = 0; done < settle; done += kBlock)
            if (!engine.process(inPtr, outPtr, kBlock)) { std::fprintf(stderr, "%s: DSP halted\n", c.id.c_str()); return 1; }

        const size_t frames = stim.audio.frames();
        const size_t latency = static_cast<size_t>(engine.latency());
        Audio out;
        out.rate = rate;
        out.ch.assign(4, std::vector<float>(frames, 0.0f));
        for (size_t done = 0; done < frames + latency;) {
            const size_t n = std::min(kBlock, frames + latency - done);
            for (size_t i = 0; i < n; ++i) {
                const size_t k = done + i;
                inBuf[0][i] = k < frames ? stim.audio.ch[0][k] : 0.0f;
                inBuf[1][i] = k < frames ? stim.audio.ch[1][k] : 0.0f;
            }
            if (!engine.process(inPtr, outPtr, n)) { std::fprintf(stderr, "%s: DSP halted\n", c.id.c_str()); return 1; }
            for (size_t i = 0; i < n; ++i) {
                const size_t k = done + i;
                if (k >= latency) for (int ch = 0; ch < 4; ++ch) out.ch[ch][k - latency] = outBuf[ch][i];
            }
            done += n;
        }
        if (!writeWavFloat(outDir + "/" + c.id + ".wav", out, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
        uint8_t sl[12][6];
        engine.sliderValues(sl);
        std::fprintf(index, "%s\t%s\t%02X\t%s\t%.3f\t%u\t%d\t%.4f\t%s\t%s\t%02X\n", c.id.c_str(), prog->name, prog->id,
                     c.stimulus.c_str(), c.seconds, rate, engine.loopLen(), engine.dspRate(),
                     c.settings.empty() ? "-" : c.settings.c_str(), sliderHex(sl).c_str(), engine.options());
        std::fflush(index);
        std::fprintf(stderr, "%-24s %-13s loop %3d  %.0f Hz  underruns %u\n", c.id.c_str(), prog->name, engine.loopLen(),
                     engine.dspRate(), engine.underruns());
    }
    std::fclose(index);
    return 0;
}
