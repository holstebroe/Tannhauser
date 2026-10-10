// tearwash_core_test: checks of the virtual 224 core and the native program networks
// (docs/tearwash/04 §5). W1–W3 run everywhere; the bit-exact network checks (W2b) need a
// capture made locally with `tearwash_oracle --capture` (build/capture/<id>.bin) and are
// skipped without one.

#include "tearwash/engine/Programs.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

using namespace tearwash;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...) do { if (cond) { ++g_pass; } else { ++g_fail; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

// W1: multiply-accumulate against the documented hardware results (x·c/32, ZERO then one MAC).
static void testMac() {
    struct V { uint16_t x; int c; uint16_t want; };
    const V v[] = {
        { 0x5555, -8, 0xEAAA }, { 0xAAAA, -8, 0x1555 }, { 0x5555, 16, 0x2AAA }, { 0xAAAA, 16, 0xD555 },
        { 0x5555, 21, 0x3800 }, { 0x6666, 21, 0x4333 }, { 0x9999, 21, 0xBCCD }, { 0xAAAA, 21, 0xC800 },
        { 0x5555, -21, 0xC7FF }, { 0x6666, -21, 0xBCCC }, { 0x9999, -21, 0x4332 }, { 0xAAAA, -21, 0x37FF },
        { 0x5555, 32, 0x5555 }, { 0xAAAA, 32, 0xAAAA }, { 0x5555, -32, 0xAAA9 }, { 0xAAAA, -32, 0x5554 },
        { 0x5555, -40, 0x9554 }, { 0xAAAA, -40, 0x6AAA }, { 0x5555, 42, 0x7000 }, { 0x6666, 42, 0x7FFF },
        { 0x9999, 42, 0x8000 }, { 0xAAAA, 42, 0x9000 }, { 0x5555, -42, 0x8FFF }, { 0x6666, -42, 0x8000 },
        { 0x9999, -42, 0x7FFF }, { 0xAAAA, -42, 0x6FFF }, { 0x3333, 63, 0x64CE }, { 0x3FFF, 63, 0x7DFF },
        { 0xC000, 63, 0x8202 }, { 0xCCCC, 63, 0x9B33 },
    };
    for (const V& t : v) {
        Mac a;
        a.zero();
        a.add(int16_t(t.x), t.c);
        CHECK(uint16_t(a.result()) == t.want, "MAC %04X x %d/32 = %04X, want %04X", t.x, t.c, uint16_t(a.result()), t.want);
    }
}

// W3: converter quantisation steps 16/8/4/2.
static void testFpc() {
    CHECK(fpcQuantize(0x7FFF) == 0x7FF0, "fpc 7FFF");
    CHECK(fpcQuantize(0x2FFF) == 0x2FF8, "fpc 2FFF");
    CHECK(fpcQuantize(0x1FFF) == 0x1FFC, "fpc 1FFF");
    CHECK(fpcQuantize(0x0FFF) == 0x0FFE, "fpc 0FFF");
    CHECK(fpcQuantize(-1) == -2, "fpc -1");
    CHECK(fpcQuantize(int16_t(0x8000)) == int16_t(0x8000), "fpc 8000");
}

// W2b: native network vs a capture of the original program (same state, same input).
struct Capture {
    int loop = 0;
    std::vector<int16_t> mem, inL, inR;
    std::vector<int16_t> out;   // 4 per frame
    int16_t result = 0;
    int32_t acc = 0;
    uint16_t pos = 0;
    // Memory snapshots after every 100 frames (diagnostics).
    std::vector<uint16_t> snapPos;
    std::vector<std::vector<int16_t>> snaps;
};

static bool loadCapture(const std::string& path, Capture& c) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::vector<uint8_t> d;
    uint8_t buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) d.insert(d.end(), buf, buf + n);
    std::fclose(f);
    size_t p = 0;
    auto u16 = [&]() { uint16_t v = uint16_t(d[p] | (d[p + 1] << 8)); p += 2; return v; };
    auto u32 = [&]() { uint32_t lo = u16(); return lo | (uint32_t(u16()) << 16); };
    if (d.size() < 8 || std::string(d.begin(), d.begin() + 8) != "TWCAP001") return false;
    p = 8;
    c.loop = int(u32());
    p += 512;                        // program image: not needed by the native network
    for (int k = 0; k < 4; ++k) u16();
    c.result = int16_t(u16());
    c.acc = int32_t(u32());
    c.pos = u16();
    c.mem.resize(65536);
    for (auto& w : c.mem) w = int16_t(u16());
    const uint32_t frames = u32();
    for (uint32_t i = 0; i < frames; ++i) { c.inL.push_back(int16_t(u16())); c.inR.push_back(int16_t(u16())); }
    const uint32_t outs = u32();
    for (uint32_t i = 0; i < 4 * outs; ++i) c.out.push_back(int16_t(u16()));
    if (p + 4 <= d.size()) {
        const uint32_t ns = u32();
        for (uint32_t k = 0; k < ns; ++k) {
            c.snapPos.push_back(u16());
            c.snaps.emplace_back(65536);
            for (auto& w : c.snaps.back()) w = int16_t(u16());
        }
    }
    return true;
}

// Runs `prog` from the capture state with input lag `li`; returns the first mismatching frame
// (or frames when all match) for output lag `lo`.
static size_t compare(Program& prog, const Capture& c, int li, int lo, size_t limit, std::string* why) {
    CoreState s;
    std::copy(c.mem.begin(), c.mem.end(), s.mem.data());
    s.mem.setPosition(c.pos);
    prog.reset();
    prog.loadPipeline(c.acc, c.result);
    const size_t frames = std::min(limit, c.out.size() / 4);
    for (size_t i = 0; i < frames; ++i) {
        const long k = long(i) + li;
        s.inL = (k >= 0 && size_t(k) < c.inL.size()) ? fpcQuantize(c.inL[size_t(k)]) : 0;
        s.inR = (k >= 0 && size_t(k) < c.inR.size()) ? fpcQuantize(c.inR[size_t(k)]) : 0;
        prog.tick(s);
        const long j = long(i) + lo;
        if (j < 0 || size_t(j) >= frames) continue;
        for (int ch = 0; ch < 4; ++ch) {
            const int16_t want = c.out[4 * size_t(j) + ch], got = fpcQuantize(s.dac[ch]);
            if (want != got) {
                if (why) {
                    char b[96];
                    std::snprintf(b, sizeof b, "frame %zu DAC %c: %d, want %d", i, "ABCD"[ch], got, want);
                    *why = b;
                }
                return i;
            }
        }
    }
    return frames;
}

static void testCapture(const char* id, Program& prog) {
    Capture c;
    const std::string path = std::string("build/capture/") + id + ".bin";
    if (!loadCapture(path, c)) { std::printf("skip  W2b %s: no %s\n", id, path.c_str()); return; }
    CHECK(c.loop == prog.loopLength(), "%s loop %d, network %d", id, c.loop, prog.loopLength());
    int bestLi = 0, bestLo = 0;
    size_t best = 0;
    for (int li = -2; li <= 2; ++li)
        for (int lo = -2; lo <= 2; ++lo) {
            const size_t m = compare(prog, c, li, lo, 4000, nullptr);
            if (m > best) { best = m; bestLi = li; bestLo = lo; }
        }
    std::string why;
    const size_t frames = c.out.size() / 4;
    const size_t m = compare(prog, c, bestLi, bestLo, frames, &why);
    CHECK(m == frames, "%s bit-exact for %zu of %zu frames (input lag %d, output lag %d): %s", id, m, frames, bestLi,
          bestLo, why.c_str());
    if (m == frames) std::printf("ok    W2b %s bit-exact over %zu frames (lags %d/%d)\n", id, frames, bestLi, bestLo);
    if (m != frames && !c.snaps.empty() && std::getenv("TW_DIAG")) {
        // Re-run snapshot by snapshot; report the first one that differs and its offsets.
        CoreState s;
        std::copy(c.mem.begin(), c.mem.end(), s.mem.data());
        s.mem.setPosition(c.pos);
        prog.reset();
        prog.loadPipeline(c.acc, c.result);
        size_t i = 0;
        for (size_t k = 0; k < c.snaps.size(); ++k) {
            for (; i < 100 * (k + 1); ++i) {
                const long j = long(i) + bestLi;
                s.inL = (j >= 0 && size_t(j) < c.inL.size()) ? fpcQuantize(c.inL[size_t(j)]) : 0;
                s.inR = (j >= 0 && size_t(j) < c.inR.size()) ? fpcQuantize(c.inR[size_t(j)]) : 0;
                prog.tick(s);
            }
            int shown = 0;
            for (int off = 0; off < 65536; ++off) {
                const int16_t want = c.snaps[k][uint16_t(c.snapPos[k] - off)];
                const int16_t got = s.mem.data()[uint16_t(s.mem.position() - off)];
                if (want != got && shown++ < 30) std::printf("diag: after %zu frames offset %5d: %6d want %6d\n", i, off, got, want);
            }
            if (shown) { std::printf("diag: %d offsets differ (positions %u / %u)\n", shown, s.mem.position(), c.snapPos[k]); break; }
        }
    }
}

int main() {
    testMac();
    testFpc();
    ConcertHall ch;
    testCapture("01", ch);
    std::printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
