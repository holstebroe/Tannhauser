// tearwash_oracle: render the Tearwash calibration set through the original Lexicon 224XL V8.21
// firmware, emulated by BlueBox (github.com/jimbattin/bluebox), as reference audio for
// tearwash_calib (docs/tearwash/04_VALIDATION.md).
//
// Reference-only tool: it links BlueBox's lexcore from a local checkout and reads the user's own
// ROM images. Neither BlueBox code nor ROM data is part of this repository or of any plugin.
//
// usage: tearwash_oracle --rom-dir DIR --set FILE --out DIR [--rate HZ] [--only ID]
//        tearwash_oracle --rom-dir DIR --list
//        tearwash_oracle --rom-dir DIR --capture PROGRAM_ID_HEX FRAMES OUT.bin [P.S=HH ...]
//        tearwash_oracle --rom-dir DIR --image PROGRAM_ID_HEX [P.S=HH ...] [opt=MASK:BITS]
//        tearwash_oracle --rom-dir DIR --trace PROGRAM_ID_HEX STEP STIMULUS SECONDS [P.S=HH ...] [opt=..]
//
// --trace runs a stimulus (generated at the DSP rate, no emphasis) with the factory options and
// prints every change of program step STEP's coefficient (time, m).
//
// --image prints the loaded program image (hex, 4000h-41FFh) after the slider moves have been
// applied and 2 s of silence, with the live options off unless opt= says otherwise.
//
// --capture boots a program with MODE ENH, DECAY OPT and DYN DECAY forced off (so the 8080
// leaves the DSP program alone), snapshots the whole DSP state at a sample boundary, then runs
// FRAMES samples of a deterministic input at the DSP rate and records the DAC words. The file
// feeds tearwash_core_test's bit-exact check of the native networks (docs/tearwash/04 §5 W2).
// It contains the loaded program image, so it stays under build/.
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

// Machine with access to the DSP state (its members are protected for subclasses).
class ProbeMachine : public Machine {
public:
    Dsp& core() { return dsp; }
};

// Deterministic DSP-rate input: noise bursts and a quiet spell, different in L and R.
struct CaptureIo final : SampleIo {
    std::vector<int16_t> inL, inR;
    std::vector<DacFrame> out;
    size_t next = 0;
    bool nextInput(int16_t& l, int16_t& r) override {
        if (next >= inL.size()) { l = r = 0; return true; }
        l = inL[next]; r = inR[next]; ++next;
        return true;
    }
    void frame(const DacFrame& d) override { out.push_back(d); }
};

void put16(FILE* f, uint16_t v) { std::fputc(v & 0xFF, f); std::fputc(v >> 8, f); }
void put32(FILE* f, uint32_t v) { put16(f, uint16_t(v)); put16(f, uint16_t(v >> 16)); }

// Boot `id` with the live options forced (default: all off), apply slider moves through the
// firmware and settle 2 s of silence.
bool bootWith(ProbeMachine& m, const RomImage& roms, int16_t* dmem, unsigned id, const std::vector<Move>& moves,
              unsigned optMask, unsigned optBits) {
    m.reset(roms, dmem);
    m.setBootOptions(static_cast<uint8_t>(optMask), static_cast<uint8_t>(optBits));
    const char* err = nullptr;
    if (!m.boot(static_cast<uint8_t>(id), &err)) { std::fprintf(stderr, "boot: %s\n", err); return false; }
    for (const Move& mv : moves)
        if (!m.moveSlider(mv.page, mv.slider - 1, static_cast<uint8_t>(mv.pos))) {
            std::fprintf(stderr, "slider %d.%d not available\n", mv.page, mv.slider);
            return false;
        }
    CaptureIo io;
    io.inL.assign(1, 0); io.inR.assign(1, 0);
    m.run(static_cast<size_t>(2.0 * 32000), io);
    return true;
}

std::string sliderHex(const uint8_t (&v)[12][6]);
constexpr unsigned kAllOpts = Machine::kOptModeEnh | Machine::kOptDecayOpt | Machine::kOptDynDecay;

int image(const RomImage& roms, unsigned id, const std::vector<Move>& moves, unsigned optMask, unsigned optBits) {
    static int16_t dmem[65536];
    static ProbeMachine m;
    if (!bootWith(m, roms, dmem, id, moves, optMask, optBits)) return 1;
    uint8_t sl[12][6];
    m.sliderValues(sl);
    std::printf("# registers %s options %02X\n", sliderHex(sl).c_str(), m.options());
    for (int i = 0; i < 512; i += 32) {
        std::printf("%04X:", 0x4000 + i);
        for (int k = 0; k < 32; ++k) std::printf(" %02X", m.core().wcs[i + k]);
        std::printf("\n");
    }
    return 0;
}

int trace(const RomImage& roms, unsigned id, int step, const std::string& stim, double seconds,
          const std::vector<Move>& moves, unsigned optMask, unsigned optBits) {
    static int16_t dmem[65536];
    static ProbeMachine m;
    if (!bootWith(m, roms, dmem, id, moves, optMask, optBits)) return 1;
    const double rate = Machine::kMasterHz / double(Machine::kTicksPerDspStep) / m.loopLen;
    Stimulus st = makeStimulus(stim, static_cast<unsigned>(rate), seconds);
    CaptureIo io;
    for (size_t i = 0; i < st.audio.frames(); ++i) {
        io.inL.push_back(int16_t(std::lround(st.audio.ch[0][i] * 32767.0)));
        io.inR.push_back(int16_t(std::lround(st.audio.ch[1][i] * 32767.0)));
    }
    int last = -1;
    Dsp& d = m.core();
    for (size_t i = 0; i < io.inL.size(); i += 32) {
        m.run(32, io);
        const int mm = (~d.wcs[4 * step + 3] >> 2) & 0x3F;
        if (mm != last) { std::printf("%.3f %d\n", double(i) / rate, mm); last = mm; }
    }
    return 0;
}

int capture(const RomImage& roms, unsigned id, size_t frames, const std::string& path, const std::vector<Move>& moves) {
    static int16_t dmem[65536];
    static ProbeMachine m;
    if (!bootWith(m, roms, dmem, id, moves, kAllOpts, 0)) return 1;
    CaptureIo io;
    io.inL.clear(); io.inR.clear(); io.out.clear(); io.next = 0;
    uint64_t x = 0x7EA2A5225ull;
    for (size_t i = 0; i < frames; ++i) {
        x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
        const uint64_t r = x * 0x2545F4914F6CDD1Dull;
        const bool loud = (i % 8000) < 2000;
        const int amp = loud ? 6000 : 300;
        io.inL.push_back(int16_t(int(int16_t(r >> 48)) * amp / 32768));
        io.inR.push_back(int16_t(int(int16_t(r >> 32)) * amp / 32768));
    }
    Dsp& d = m.core();
    uint8_t wcs0[512];
    std::copy(d.wcs, d.wcs + 512, wcs0);
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { std::fprintf(stderr, "cannot write %s\n", path.c_str()); return 1; }
    std::fwrite("TWCAP002", 1, 8, f);
    put32(f, uint32_t(m.loopLen));
    std::fwrite(d.wcs, 1, 512, f);
    uint8_t sl[12][6];
    m.sliderValues(sl);
    std::fwrite(sl, 1, 72, f);                 // firmware registers, pages 1-12
    std::fputc(m.options(), f);
    for (int k = 0; k < 4; ++k) put16(f, uint16_t(d.reg[k]));
    put16(f, uint16_t(d.result));
    put32(f, uint32_t(d.acc));
    put16(f, d.pos);
    for (int k = 0; k < 65536; ++k) put16(f, uint16_t(dmem[k]));
    // Memory snapshots every 100 frames over the first 2100, for locating divergences.
    const size_t first = std::min<size_t>(frames, 2100);
    std::vector<std::vector<int16_t>> snaps;
    std::vector<uint16_t> snapPos;
    for (size_t i = 0; i < first; i += 100) {
        m.run(100, io);
        snaps.emplace_back(dmem, dmem + 65536);
        snapPos.push_back(d.pos);
    }
    if (frames > io.out.size()) m.run(frames - io.out.size(), io);
    const bool same = std::equal(wcs0, wcs0 + 512, d.wcs);
    put32(f, uint32_t(frames));
    for (size_t i = 0; i < frames; ++i) { put16(f, uint16_t(io.inL[i])); put16(f, uint16_t(io.inR[i])); }
    put32(f, uint32_t(io.out.size()));
    for (const auto& fr : io.out) for (int k = 0; k < 4; ++k) put16(f, uint16_t(fr[k]));
    put32(f, uint32_t(snaps.size()));
    for (size_t k = 0; k < snaps.size(); ++k) {
        put16(f, snapPos[k]);
        for (int w = 0; w < 65536; ++w) put16(f, uint16_t(snaps[k][size_t(w)]));
    }
    std::fclose(f);
    std::fprintf(stderr, "program %02X loop %d: %zu frames, %zu DAC frames, program %s during the run\n", id, m.loopLen,
                 frames, io.out.size(), same ? "unchanged" : "CHANGED");
    return 0;
}


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
    std::string romDir, setFile, outDir, only, capturePath;
    unsigned captureId = 0, imageId = 0, optMask = 0xC1, optBits = 0;
    bool imageMode = false, traceMode = false;
    int traceStep = 0;
    std::string traceStim;
    double traceSec = 0;
    std::vector<Move> moves;
    size_t captureFrames = 0;
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
        else if (a == "--capture" && i + 3 < argc) {
            captureId = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 16));
            captureFrames = static_cast<size_t>(std::atol(argv[++i]));
            capturePath = argv[++i];
        }
        else if (a == "--trace" && i + 4 < argc) {
            traceMode = true;
            imageId = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 16));
            traceStep = std::atoi(argv[++i]);
            traceStim = argv[++i];
            traceSec = std::atof(argv[++i]);
            optMask = 0; optBits = 0;          // factory options unless opt= is given
        }
        else if (a == "--image" && i + 1 < argc) { imageMode = true; imageId = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 16)); }
        else if (a.rfind("opt=", 0) == 0) std::sscanf(a.c_str() + 4, "%x:%x", &optMask, &optBits);
        else if (a.find('=') != std::string::npos && a[0] != '-') {
            Move mv{};
            if (std::sscanf(a.c_str(), "%d.%d=%x", &mv.page, &mv.slider, &mv.pos) != 3) { std::fprintf(stderr, "bad %s\n", a.c_str()); return 2; }
            moves.push_back(mv);
        }
        else { std::fprintf(stderr, "unknown argument %s\n", a.c_str()); return 2; }
    }
    if (romDir.empty() || (!list && !imageMode && !traceMode && capturePath.empty() && (setFile.empty() || outDir.empty()))) {
        std::fprintf(stderr, "usage: tearwash_oracle --rom-dir DIR --set FILE --out DIR [--rate HZ] [--only ID]\n"
                             "       tearwash_oracle --rom-dir DIR --list\n");
        return 2;
    }

    static uint8_t sbc[0x1800], nvs[0x8000];
    std::string err;
    if (!loadRomFiles(romDir, sbc, nvs, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    const RomImage roms{ sbc, nvs };
    if (traceMode) return trace(roms, imageId, traceStep, traceStim, traceSec, moves, optMask, optBits);
    if (imageMode) return image(roms, imageId, moves, optMask, optBits);
    if (!capturePath.empty()) return capture(roms, captureId, captureFrames, capturePath, moves);
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
