// tearwash_calib: compare reverb candidates against the Lexicon oracle renders
// (docs/tearwash/04_VALIDATION.md). Reads ORACLE_DIR/index.tsv written by tools/oracle,
// regenerates each stimulus, renders every candidate with matched settings, measures both and
// writes a markdown report plus a per-case TSV.
//
// usage: tearwash_calib ORACLE_DIR [--report FILE.md] [--tsv FILE.tsv] [--candidate NAME]
//                       [--wav-dir DIR] [--only SUBSTRING]
// Candidates: plate (the Tannhäuser Dattorro plate, spec 03 §12), tw (the Tearwash engine).

#include "core/Effects.hpp"
#include "tearwash/engine/Tearwash.hpp"
#include "tearwash/analysis/Metrics.hpp"
#include "tearwash/analysis/Stimuli.hpp"
#include "tearwash/analysis/Wav.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace tearwash;

namespace {

struct OracleCase {
    std::string id, program, programId, stimulus, settings, sliders, options;
    double seconds = 0, dspRate = 0;
    unsigned rate = 48000;
    int loop = 0;
    StimKind kind() const {
        if (stimulus == "burst") return StimKind::Burst;
        if (stimulus == "sine1k") return StimKind::Sine;
        if (stimulus.rfind("sweep", 0) == 0) return StimKind::Sweep;
        return StimKind::Impulse;
    }
    std::string fitKey() const { return program + "|" + settings; }
};

std::vector<double> toD(const std::vector<float>& v) { return std::vector<double>(v.begin(), v.end()); }

bool isIr(StimKind k) { return k == StimKind::Impulse || k == StimKind::Sweep; }

// Metrics of a stereo response to stimulus `s` (sweeps are deconvolved first).
Metrics measure(const OracleCase& c, const Stimulus& s, const std::vector<double>& L, const std::vector<double>& R) {
    if (c.kind() == StimKind::Sweep) {
        const auto& src = s.audio.ch[c.stimulus == "sweepR" ? 1 : 0];
        const std::vector<double> sweep(src.begin() + s.start, src.begin() + s.stop);
        return analyseSweep(L, R, sweep, s.start, c.rate);
    }
    return analyse(L, R, toD(s.audio.ch[0]), toD(s.audio.ch[1]), s.start, s.stop, c.rate, c.kind());
}

// --- Candidates ---------------------------------------------------------------------------------

// A reverb under test. `fit` may tune free controls so the candidate matches the oracle's
// impulse metrics as well as it can (for engines without the 224's controls); `render` returns
// the wet stereo output for a 2-channel stimulus.
class Candidate {
public:
    virtual ~Candidate() = default;
    virtual std::string name() const = 0;
    virtual std::string describe() const = 0;
    virtual void fit(const OracleCase& c, const Metrics& target) = 0;
    virtual std::string fitText() const = 0;
    virtual void render(const OracleCase& c, const Stimulus& s, std::vector<double>& L, std::vector<double>& R) = 0;
    virtual void saveFit(const std::string& key) = 0;
    virtual bool loadFit(const std::string& key) = 0;
    // Whether the candidate can render this case (program and settings it implements).
    virtual bool supports(const OracleCase&) const { return true; }
};

// The current Tannhäuser reverb: Dattorro plate with free Decay / Tone / Pre-delay (0..1).
class PlateCandidate final : public Candidate {
public:
    std::string name() const override { return "plate"; }
    std::string describe() const override {
        return "Tannhäuser `PlateReverb` (Dattorro 1997 plate, host rate, mono in). Decay, Tone and Pre-delay are "
               "fitted per case to the oracle's 1 kHz decay time, tail spectrum and onset; Mix = 1, dry removed.";
    }
    void render(const OracleCase&, const Stimulus& s, std::vector<double>& L, std::vector<double>& R) override {
        run(s, decay_, tone_, pre_, L, R);
    }
    void fit(const OracleCase& c, const Metrics& t) override {
        const double fs = 48000;
        const double tgt = t.band[3].rt() > 0 ? t.band[3].rt() : t.band[3].edt;
        Stimulus imp = makeStimulus("impulse", static_cast<unsigned>(fs), std::min(c.seconds, std::max(3.0, 1.5 * tgt + 1.0)));
        // Light measurement for the search: 1 kHz band decay, tail spectrum and onset only.
        struct Quick { double rt = 0, onsetMs = 0; std::vector<double> spectrum; };
        auto measure = [&](double d, double tn, double p) {
            std::vector<double> L, R;
            run(imp, d, tn, p, L, R);
            std::vector<double> mono(L.size());
            for (size_t i = 0; i < L.size(); ++i) mono[i] = 0.5 * (L[i] + R[i]);
            Quick q;
            const DecayFit f = schroeder(bandpass(mono, 707.1, 1414.2, fs), imp.start, fs);
            q.rt = f.rt() > 0 ? f.rt() : f.edt;
            const auto wide = bandpass(mono, 80.0, 8000.0, fs);
            double peak = 0;
            for (double v : wide) peak = std::max(peak, std::fabs(v));
            size_t onset = imp.start;
            for (size_t i = imp.start; i < wide.size(); ++i) if (std::fabs(wide[i]) >= 0.1 * peak) { onset = i; break; }
            q.onsetMs = 1000.0 * double(onset - imp.start) / fs;
            const size_t a = onset + static_cast<size_t>(0.05 * fs);
            q.spectrum = bandLevels(powerSpectrum(mono, a, a + static_cast<size_t>(0.5 * fs)), fs, thirdOctaveBands());
            double ref = 0; int n = 0;
            const auto& c = thirdOctaveBands();
            for (size_t i = 0; i < c.size(); ++i) if (c[i] >= 250 && c[i] <= 4000) { ref += q.spectrum[i]; ++n; }
            for (double& v : q.spectrum) v -= ref / n;
            return q;
        };
        const double target1k = t.band[3].rt() > 0 ? t.band[3].rt() : t.band[3].edt;
        auto fitDecay = [&](double tn) {
            double lo = 0.0, hi = 1.0;
            for (int i = 0; i < 12; ++i) {
                const double mid = 0.5 * (lo + hi);
                (measure(mid, tn, 0.0).rt < target1k ? lo : hi) = mid;
            }
            return 0.5 * (lo + hi);
        };
        tone_ = 0.6;
        decay_ = fitDecay(tone_);
        double best = 1e9;
        for (int k = 0; k <= 10; ++k) {
            const double tn = 0.1 * k;
            const Quick m = measure(decay_, tn, 0.0);
            double e = 0;
            for (size_t i = 3; i + 2 < m.spectrum.size(); ++i) e += (m.spectrum[i] - t.spectrum[i]) * (m.spectrum[i] - t.spectrum[i]);
            if (e < best) { best = e; tone_ = tn; }
        }
        decay_ = fitDecay(tone_);
        const Quick m0 = measure(decay_, tone_, 0.0);
        pre_ = std::max(0.0, std::min(1.0, (t.onsetMs - m0.onsetMs) / 150.0));
    }
    std::string fitText() const override {
        char b[96];
        std::snprintf(b, sizeof b, "decay %.3f, tone %.1f, pre-delay %.3f", decay_, tone_, pre_);
        return b;
    }
    void saveFit(const std::string& key) override { fits_[key] = { decay_, tone_, pre_ }; }
    bool loadFit(const std::string& key) override {
        auto it = fits_.find(key);
        if (it == fits_.end()) return false;
        decay_ = it->second[0]; tone_ = it->second[1]; pre_ = it->second[2];
        return true;
    }

private:
    void run(const Stimulus& s, double d, double tn, double p, std::vector<double>& L, std::vector<double>& R) {
        const double fs = s.audio.rate;
        tannhauser::PlateReverb rev;
        rev.setSampleRate(fs);
        // Settle the internal mix smoother (50 ms) on silence, then remove the 0.6 dry share.
        const size_t settle = static_cast<size_t>(0.6 * fs), n = s.audio.frames();
        std::vector<float> l(settle + n, 0.0f), r(settle + n, 0.0f);
        for (size_t i = 0; i < n; ++i) { l[settle + i] = s.audio.ch[0][i]; r[settle + i] = s.audio.ch[1][i]; }
        const int blk = 256;
        for (size_t i = 0; i < l.size(); i += blk) {
            const int m = static_cast<int>(std::min<size_t>(blk, l.size() - i));
            rev.process(&l[i], &r[i], m, 1.0, d, tn, p);
        }
        L.assign(n, 0.0); R.assign(n, 0.0);
        for (size_t i = 0; i < n; ++i) {
            L[i] = l[settle + i] - 0.6 * s.audio.ch[0][i];
            R[i] = r[settle + i] - 0.6 * s.audio.ch[1][i];
        }
    }
    double decay_ = 0.6, tone_ = 0.6, pre_ = 0.0;
    std::map<std::string, std::array<double, 3>> fits_;
};

// The Tearwash engine with native 224XL program networks, factory settings (no fitting).
class TearwashCandidate final : public Candidate {
public:
    std::string name() const override { return "tw"; }
    std::string describe() const override {
        return "Tearwash 225 engine, 224XL flavour: native program networks on the virtual 224 core, 224X converter "
               "emphasis, host/core resampling, Mode Enhancement tap walker. Factory settings; Decay Optimisation not yet "
               "implemented. Nothing is fitted.";
    }
    bool supports(const OracleCase& c) const override {
        return (c.program == "CONCERT HALL") && (c.settings == "-" || c.settings == "opt=40:00");
    }
    void fit(const OracleCase&, const Metrics&) override {}
    std::string fitText() const override { return "factory"; }
    void saveFit(const std::string&) override {}
    bool loadFit(const std::string&) override { return true; }
    void render(const OracleCase& c, const Stimulus& s, std::vector<double>& L, std::vector<double>& R) override {
        tearwash::Engine e;
        e.setSampleRate(c.rate);
        e.setProgram(std::make_unique<tearwash::ConcertHall>());
        e.setModeEnhancement(c.settings.find("opt=40:00") == std::string::npos);
        const size_t lat = static_cast<size_t>(std::lround(e.latency()));
        const size_t n = s.audio.frames(), total = n + lat;
        std::vector<float> inL(total, 0.0f), inR(total, 0.0f), a(total), b(total), cc(total), d(total);
        std::copy(s.audio.ch[0].begin(), s.audio.ch[0].end(), inL.begin());
        std::copy(s.audio.ch[1].begin(), s.audio.ch[1].end(), inR.begin());
        float* outs[4] = { a.data(), b.data(), cc.data(), d.data() };
        const int blk = 256;
        for (size_t i = 0; i < total; i += blk) {
            const int m = static_cast<int>(std::min<size_t>(blk, total - i));
            float* o[4] = { outs[0] + i, outs[1] + i, outs[2] + i, outs[3] + i };
            e.process(inL.data() + i, inR.data() + i, o, m);
        }
        L.assign(a.begin() + lat, a.begin() + lat + n);
        R.assign(cc.begin() + lat, cc.begin() + lat + n);
    }
};

std::unique_ptr<Candidate> makeCandidate(const std::string& n) {
    if (n == "plate") return std::make_unique<PlateCandidate>();
    if (n == "tw") return std::make_unique<TearwashCandidate>();
    return nullptr;
}

// --- Report helpers -----------------------------------------------------------------------------

std::string f1(double v) { char b[32]; std::snprintf(b, sizeof b, v > 0 || v < 0 ? "%.2f" : "–", v); return b; }
std::string f0(double v) { char b[32]; std::snprintf(b, sizeof b, "%.0f", v); return b; }
std::string fd(double v, const char* fmt = "%.1f") { char b[32]; std::snprintf(b, sizeof b, fmt, v); return b; }
std::string pair(const std::string& a, const std::string& b) { return a + " / " + b; }

double bandRtErr(const Metrics& o, const Metrics& c) {   // mean |c/o - 1| over octave bands, %
    double s = 0; int n = 0;
    for (size_t i = 0; i < o.band.size(); ++i) {
        const double a = o.band[i].rt(), b = c.band[i].rt();
        if (a > 0 && b > 0) { s += std::fabs(b / a - 1.0); ++n; }
    }
    return n ? 100.0 * s / n : 0;
}
double specErr(const Metrics& o, const Metrics& c) {     // RMS dB over 100 Hz..10 kHz
    double s = 0; int n = 0;
    const auto& f = thirdOctaveBands();
    for (size_t i = 0; i < f.size() && i < o.spectrum.size() && i < c.spectrum.size(); ++i)
        if (f[i] >= 100 && f[i] <= 10000) { s += (o.spectrum[i] - c.spectrum[i]) * (o.spectrum[i] - c.spectrum[i]); ++n; }
    return n ? std::sqrt(s / n) : 0;
}
double nedErr(const Metrics& o, const Metrics& c) {      // RMS NED difference over the first 300 ms
    double s = 0; size_t n = std::min({ o.ned.size(), c.ned.size(), size_t(60) });
    for (size_t i = 0; i < n; ++i) s += (o.ned[i] - c.ned[i]) * (o.ned[i] - c.ned[i]);
    return n ? std::sqrt(s / n) : 0;
}
std::string bandRow(const Metrics& m) {
    std::string s;
    for (size_t i = 0; i < m.band.size(); ++i) s += (i ? " " : "") + fd(m.band[i].rt(), "%.2f");
    return s;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: tearwash_calib ORACLE_DIR [--report FILE.md] [--tsv FILE.tsv] [--candidate NAME] [--wav-dir DIR]\n");
        return 2;
    }
    const std::string dir = argv[1];
    std::string reportPath, tsvPath, wavDir, only, candName = "plate";
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--report" && i + 1 < argc) reportPath = argv[++i];
        else if (a == "--tsv" && i + 1 < argc) tsvPath = argv[++i];
        else if (a == "--candidate" && i + 1 < argc) candName = argv[++i];
        else if (a == "--wav-dir" && i + 1 < argc) wavDir = argv[++i];
        else if (a == "--only" && i + 1 < argc) only = argv[++i];
        else { std::fprintf(stderr, "unknown argument %s\n", a.c_str()); return 2; }
    }
    auto cand = makeCandidate(candName);
    if (!cand) { std::fprintf(stderr, "unknown candidate %s\n", candName.c_str()); return 2; }

    std::ifstream idx(dir + "/index.tsv");
    if (!idx) { std::fprintf(stderr, "cannot read %s/index.tsv\n", dir.c_str()); return 1; }
    std::vector<OracleCase> cases;
    std::string line;
    std::getline(idx, line);
    while (std::getline(idx, line)) {
        std::istringstream is(line);
        OracleCase c;
        std::string sec, rate, loop, dsp;
        std::getline(is, c.id, '\t'); std::getline(is, c.program, '\t'); std::getline(is, c.programId, '\t');
        std::getline(is, c.stimulus, '\t'); std::getline(is, sec, '\t'); std::getline(is, rate, '\t');
        std::getline(is, loop, '\t'); std::getline(is, dsp, '\t'); std::getline(is, c.settings, '\t');
        std::getline(is, c.sliders, '\t'); std::getline(is, c.options, '\t');
        if (c.id.empty() || (!only.empty() && c.id.find(only) == std::string::npos)) continue;
        if (!cand->supports(c)) continue;
        c.seconds = std::atof(sec.c_str()); c.rate = static_cast<unsigned>(std::atoi(rate.c_str()));
        c.loop = std::atoi(loop.c_str()); c.dspRate = std::atof(dsp.c_str());
        cases.push_back(c);
    }
    // Impulse cases first, so other stimuli can reuse the fit of the same program and settings.
    std::stable_sort(cases.begin(), cases.end(), [](const OracleCase& a, const OracleCase& b) {
        return isIr(a.kind()) > isIr(b.kind());
    });

    struct Result { OracleCase c; Metrics o, k; std::string fit; };
    std::vector<Result> res;
    for (const OracleCase& c : cases) {
        Audio oa;
        std::string err;
        if (!readWav(dir + "/" + c.id + ".wav", oa, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
        const Stimulus s = makeStimulus(c.stimulus, c.rate, c.seconds);
        if (!s.valid || oa.ch.size() < 3) { std::fprintf(stderr, "%s: bad case\n", c.id.c_str()); return 1; }
        // The 224's stereo pair is DAC A (left) and DAC C (right).
        const Metrics o = measure(c, s, toD(oa.ch[0]), toD(oa.ch[2]));
        if (isIr(c.kind())) {
            if (!cand->loadFit(c.fitKey())) { cand->fit(c, o); cand->saveFit(c.fitKey()); }
        } else if (!cand->loadFit(c.fitKey()) && !cand->loadFit(c.program + "|-")) {
            std::fprintf(stderr, "%s: no impulse case to fit from\n", c.id.c_str());
            continue;
        }
        std::vector<double> L, R;
        cand->render(c, s, L, R);
        const Metrics k = measure(c, s, L, R);
        if (!wavDir.empty()) {
            Audio w; w.rate = c.rate; w.ch = { std::vector<float>(L.begin(), L.end()), std::vector<float>(R.begin(), R.end()) };
            writeWavFloat(wavDir + "/" + c.id + "_" + cand->name() + ".wav", w, err);
        }
        res.push_back({ c, o, k, cand->fitText() });
        std::fprintf(stderr, "%-30s rt1k %5.2f / %5.2f  bandRT %5.1f%%  spec %4.1f dB  [%s]\n", c.id.c_str(), o.band[3].rt(),
                     k.band[3].rt(), bandRtErr(o, k), specErr(o, k), cand->fitText().c_str());
    }
    std::sort(res.begin(), res.end(), [](const Result& a, const Result& b) { return a.c.id < b.c.id; });
    auto find = [&](const std::string& id) -> const Result* {
        for (const auto& r : res) if (r.c.id == id) return &r;
        return nullptr;
    };

    if (!tsvPath.empty()) {
        FILE* t = std::fopen(tsvPath.c_str(), "w");
        if (t) {
            std::fprintf(t, "id\tside");
            for (double f : octaveBands()) std::fprintf(t, "\trt%.0f", f);
            std::fprintf(t, "\tedt\tonset_ms\tned_mean\tmix_s\tc50\tiacc\thf_edge\tmod_db\tgain_db\n");
            for (const auto& r : res)
                for (int side = 0; side < 2; ++side) {
                    const Metrics& m = side ? r.k : r.o;
                    std::fprintf(t, "%s\t%s", r.c.id.c_str(), side ? cand->name().c_str() : "oracle");
                    for (const auto& b : m.band) std::fprintf(t, "\t%.3f", b.rt());
                    std::fprintf(t, "\t%.3f\t%.1f\t%.3f\t%.3f\t%.1f\t%.3f\t%.0f\t%.1f\t%.1f\n", m.broad.edt, m.onsetMs, m.nedMean,
                                 m.mixTime, m.c50, m.iacc, m.hfEdge, m.modDb, m.gainDb);
                }
            std::fclose(t);
        }
    }

    // --- Markdown report ---
    std::ostringstream o;
    char date[32];
    const std::time_t now = std::time(nullptr);
    std::strftime(date, sizeof date, "%Y-%m-%d", std::gmtime(&now));
    o << "# Tearwash calibration report: `" << cand->name() << "` vs 224XL V8.21 oracle\n\n";
    o << "Generated by `tearwash_calib` on " << date << " from `" << dir << "` (" << res.size()
      << " cases). Metrics and scores are defined in `docs/tearwash/04_VALIDATION.md`.\n\n";
    o << "**Candidate.** " << cand->describe() << "\n\n";
    o << "**Oracle.** BlueBox running the original 224XL V8.21 firmware; stereo pair = DAC A / DAC C; "
         "DC removed (the hardware's output transformers block the ARU's truncation offset). Values are shown "
         "as *oracle / candidate*; – means not measurable (decay range too short).\n\n";

    // Summary over factory impulse cases.
    double sBand = 0, sSpec = 0, sNed = 0, sIacc = 0, sEdt = 0;
    int nF = 0;
    for (const auto& r : res) {
        if (r.c.id.rfind("f_", 0) || r.c.stimulus != "sweep") continue;
        sBand += bandRtErr(r.o, r.k); sSpec += specErr(r.o, r.k); sNed += nedErr(r.o, r.k);
        sIacc += std::fabs(r.o.iacc - r.k.iacc);
        if (r.o.broad.edt > 0 && r.k.broad.edt > 0) sEdt += 100.0 * std::fabs(r.k.broad.edt / r.o.broad.edt - 1.0);
        ++nF;
    }
    const Result* ms = find("f_concert_hall_sine");
    o << "## Summary (22 factory programs, sweep-derived impulse responses)\n\n";
    o << "| Score | Value | Meaning |\n| --- | --- | --- |\n";
    if (nF) {
        o << "| Band RT error | " << fd(sBand / nF) << " % | mean over programs of mean |RT_c/RT_o − 1| over octave bands 125 Hz–8 kHz (1 kHz is fitted) |\n";
        o << "| EDT error | " << fd(sEdt / nF) << " % | broadband early decay time |\n";
        o << "| Tail spectrum error | " << fd(sSpec / nF) << " dB | RMS third-octave difference 100 Hz–10 kHz, level-normalised |\n";
        o << "| Echo density error | " << fd(sNed / nF, "%.3f") << " | RMS NED difference over the first 300 ms |\n";
        o << "| Stereo correlation error | " << fd(sIacc / nF, "%.3f") << " | mean |IACC_c − IACC_o|, late field |\n";
    }
    if (ms) o << "| Modulation (CONCERT HALL) | " << pair(fd(ms->o.modDb), fd(ms->k.modDb)) << " dB | sideband/core energy of a steady 1 kHz tone |\n";
    o << "\n";

    o << "## 1. Factory programs (sweep-derived impulse responses)\n\n";
    o << "| Program | Fit | RT 1 kHz s | Band RT err | EDT s | Onset ms | NED 50–300 ms | Mixing s | C50 dB | IACC | Spec err dB | HF edge Hz |\n";
    o << "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |\n";
    for (const auto& r : res) {
        if (r.c.id.rfind("f_", 0) || r.c.stimulus != "sweep") continue;
        o << "| " << r.c.program << " | " << r.fit << " | " << pair(f1(r.o.band[3].rt()), f1(r.k.band[3].rt())) << " | "
          << fd(bandRtErr(r.o, r.k)) << " % | " << pair(f1(r.o.broad.edt), f1(r.k.broad.edt)) << " | "
          << pair(f0(r.o.onsetMs), f0(r.k.onsetMs)) << " | " << pair(fd(r.o.nedMean, "%.2f"), fd(r.k.nedMean, "%.2f")) << " | "
          << pair(f1(r.o.mixTime), f1(r.k.mixTime)) << " | " << pair(fd(r.o.c50), fd(r.k.c50)) << " | "
          << pair(fd(r.o.iacc, "%.2f"), fd(r.k.iacc, "%.2f")) << " | " << fd(specErr(r.o, r.k)) << " | "
          << pair(f0(r.o.hfEdge), f0(r.k.hfEdge)) << " |\n";
    }
    o << "\n### Octave-band decay times (s), 125 Hz … 8 kHz\n\n| Program | Oracle | Candidate |\n| --- | --- | --- |\n";
    for (const auto& r : res)
        if (!r.c.id.rfind("f_", 0) && r.c.stimulus == "sweep")
            o << "| " << r.c.program << " | " << bandRow(r.o) << " | " << bandRow(r.k) << " |\n";

    o << "\n### Tail spectra (dB re 250 Hz–4 kHz mean), selected bands\n\n| Program | Side |";
    const std::vector<double> showF = { 63, 125, 250, 500, 1000, 2000, 4000, 6300, 8000, 10000, 12500 };
    for (double f : showF) o << " " << f0(f) << " |";
    o << "\n| --- | --- |";
    for (size_t i = 0; i < showF.size(); ++i) o << " --- |";
    o << "\n";
    for (const auto& r : res) {
        if (r.c.id.rfind("f_", 0) || r.c.stimulus != "sweep") continue;
        for (int side = 0; side < 2; ++side) {
            const Metrics& m = side ? r.k : r.o;
            o << "| " << (side ? "" : r.c.program) << " | " << (side ? "cand." : "oracle") << " |";
            for (double f : showF) {
                const auto& c = thirdOctaveBands();
                for (size_t i = 0; i < c.size(); ++i) if (c[i] == f) o << " " << fd(m.spectrum[i]) << " |";
            }
            o << "\n";
        }
    }

    auto sweep = [&](const std::string& title, const std::string& prefix, const std::string& note) {
        o << "\n## " << title << "\n\n" << note << "\n\n";
        o << "| Case | Settings | Fit | RT oracle 125…8k | RT cand. 125…8k | EDT s | NED | Mixing s | C50 dB | HF edge Hz | Onset ms |\n";
        o << "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |\n";
        for (const auto& r : res) {
            if (r.c.id.rfind(prefix, 0)) continue;
            o << "| " << r.c.id << " | " << r.c.settings << " | " << r.fit << " | " << bandRow(r.o) << " | " << bandRow(r.k)
              << " | " << pair(f1(r.o.broad.edt), f1(r.k.broad.edt)) << " | " << pair(fd(r.o.nedMean, "%.2f"), fd(r.k.nedMean, "%.2f"))
              << " | " << pair(f1(r.o.mixTime), f1(r.k.mixTime)) << " | " << pair(fd(r.o.c50), fd(r.k.c50)) << " | "
              << pair(f0(r.o.hfEdge), f0(r.k.hfEdge)) << " | " << pair(f0(r.o.onsetMs), f0(r.k.onsetMs)) << " |\n";
        }
    };
    sweep("2. Decay law (LF = MID DECAY swept)", "s_concert_hall_dcy",
          "CONCERT HALL; slider positions are LARC hex codes (s = code >> 3). The candidate's Decay is re-fitted per row.");
    sweep("2b. Decay law, PLATE", "s_plate_dcy", "");
    sweep("2c. Decay law, ROOM", "s_room_dcy", "");
    sweep("3. Split-band decay (LF C0, MID 40, CROSSOVER swept)", "s_concert_hall_xov",
          "The candidate has a single decay; its fit can only follow the 1 kHz band.");
    sweep("4. Treble decay", "s_concert_hall_tre", "");
    sweep("5. Depth", "s_concert_hall_dep", "");
    sweep("6. Diffusion", "s_concert_hall_dif", "The candidate's diffusion is fixed.");
    sweep("7. Predelay", "s_concert_hall_pd", "");

    o << "\n## 7b. Measurement cross-check: raw impulse vs sweep\n\n"
         "A single-sample impulse reaches only ~40 dB above the 16-bit core's truncation floor; the sweep "
         "(3 s, 20 Hz–20 kHz, −18 dBFS, Farina deconvolution) gains about 35 dB. Oracle values.\n\n"
         "| Case | RT impulse 125…8k | RT sweep 125…8k | NED impulse / sweep |\n| --- | --- | --- | --- |\n";
    for (const auto& r : res) {
        if (r.c.stimulus != "impulse") continue;
        const std::string base = r.c.id.substr(0, r.c.id.size() - 4);   // x_imp -> x
        if (const Result* sw = find(base + "_swp"))
            o << "| " << base << " | " << bandRow(r.o) << " | " << bandRow(sw->o) << " | "
              << pair(fd(r.o.nedMean, "%.2f"), fd(sw->o.nedMean, "%.2f")) << " |\n";
    }

    o << "\n## 8. Interrupted noise (burst): decay and steady-state spectrum\n\n";
    o << "| Case | RT oracle 125…8k | RT cand. 125…8k | Steady spectrum err dB |\n| --- | --- | --- | --- |\n";
    for (const auto& r : res)
        if (r.c.kind() == StimKind::Burst)
            o << "| " << r.c.id << " | " << bandRow(r.o) << " | " << bandRow(r.k) << " | " << fd(specErr(r.o, r.k)) << " |\n";

    o << "\n## 9. Modulation (steady 1 kHz tone)\n\n| Case | Settings | Side/core dB oracle / cand. |\n| --- | --- | --- |\n";
    for (const auto& r : res)
        if (r.c.kind() == StimKind::Sine)
            o << "| " << r.c.id << " | " << r.c.settings << " | " << pair(fd(r.o.modDb), fd(r.k.modDb)) << " |\n";

    o << "\n## 10. Stereo (one input channel driven)\n\n| Case | IACC | Gain dB | Onset ms |\n| --- | --- | --- | --- |\n";
    for (const auto& r : res)
        if (r.c.stimulus == "sweepL" || r.c.stimulus == "sweepR")
            o << "| " << r.c.id << " | " << pair(fd(r.o.iacc, "%.2f"), fd(r.k.iacc, "%.2f")) << " | "
              << pair(fd(r.o.gainDb), fd(r.k.gainDb)) << " | " << pair(f0(r.o.onsetMs), f0(r.k.onsetMs)) << " |\n";

    const std::string rep = o.str();
    if (reportPath.empty()) std::fputs(rep.c_str(), stdout);
    else {
        std::ofstream f(reportPath);
        f << rep;
    }
    return 0;
}
