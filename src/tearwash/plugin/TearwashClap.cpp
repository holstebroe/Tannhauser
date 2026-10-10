#include "TearwashClap.hpp"
#include "TwGui.hpp"
#include <clap/ext/latency.h>
#include <clap/ext/state.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#if defined(__SSE__) || defined(_M_X64)
#include <xmmintrin.h>
#define TEARWASH_HAVE_SSE 1
#endif

namespace tearwash {

// A C++ exception must never cross into the host.
template <class R, class F>
static R guarded(R fallback, F&& f) noexcept {
    try { return f(); } catch (...) { return fallback; }
}
template <class F>
static void guardedVoid(F&& f) noexcept {
    try { f(); } catch (...) {}
}

#ifdef TEARWASH_HAVE_SSE
struct ScopedFlushDenormals {
    unsigned saved = _mm_getcsr();
    ScopedFlushDenormals() { _mm_setcsr(saved | 0x8040u); }
    ~ScopedFlushDenormals() { _mm_setcsr(saved); }
};
#else
struct ScopedFlushDenormals {};
#endif

static TearwashClap* self(const clap_plugin_t* p) { return static_cast<TearwashClap*>(p->plugin_data); }

static const clap_plugin_audio_ports_t g_audioPorts = {
    [](const clap_plugin_t*, bool) -> uint32_t { return 1; },
    [](const clap_plugin_t*, uint32_t index, bool isInput, clap_audio_port_info_t* info) -> bool {
        if (index != 0 || !info) return false;
        info->id = 0;
        std::snprintf(info->name, sizeof(info->name), "%s", isInput ? "Input" : "Output");
        info->channel_count = 2;
        info->flags = CLAP_AUDIO_PORT_IS_MAIN;
        info->port_type = CLAP_PORT_STEREO;
        info->in_place_pair = CLAP_INVALID_ID;
        return true;
    }
};

static const clap_plugin_params_t g_params = {
    [](const clap_plugin_t* p) -> uint32_t { return guarded<uint32_t>(0, [&] { return self(p)->paramsCount(); }); },
    [](const clap_plugin_t* p, uint32_t i, clap_param_info_t* info) -> bool {
        return guarded(false, [&] { return self(p)->paramsInfo(i, info); });
    },
    [](const clap_plugin_t* p, clap_id id, double* v) -> bool {
        return guarded(false, [&] { return self(p)->paramsValue(id, v); });
    },
    [](const clap_plugin_t* p, clap_id id, double v, char* buf, uint32_t cap) -> bool {
        return guarded(false, [&] { return self(p)->paramsValueToText(id, v, buf, cap); });
    },
    [](const clap_plugin_t* p, clap_id id, const char* text, double* v) -> bool {
        return guarded(false, [&] { return self(p)->paramsTextToValue(id, text, v); });
    },
    [](const clap_plugin_t* p, const clap_input_events_t* in, const clap_output_events_t* out) {
        guardedVoid([&] { ScopedFlushDenormals ftz; self(p)->paramsFlush(in, out); });
    }
};

static const clap_plugin_state_t g_state = {
    [](const clap_plugin_t* p, const clap_ostream_t* s) -> bool { return guarded(false, [&] { return self(p)->stateSave(s); }); },
    [](const clap_plugin_t* p, const clap_istream_t* s) -> bool { return guarded(false, [&] { return self(p)->stateLoad(s); }); },
};

static const clap_plugin_latency_t g_latency = {
    [](const clap_plugin_t* p) -> uint32_t { return guarded<uint32_t>(0, [&] { return self(p)->latency(); }); },
};

TearwashClap::TearwashClap(const clap_host_t* host) : host_(host) {
    clapPlugin_.desc = nullptr;
    clapPlugin_.plugin_data = this;
    clapPlugin_.init = [](const clap_plugin_t* p) -> bool { return guarded(false, [&] { return self(p)->init(); }); };
    clapPlugin_.destroy = [](const clap_plugin_t* p) { guardedVoid([&] { self(p)->destroy(); }); };
    clapPlugin_.activate = [](const clap_plugin_t* p, double sr, uint32_t mn, uint32_t mx) -> bool {
        return guarded(false, [&] { return self(p)->activate(sr, mn, mx); });
    };
    clapPlugin_.deactivate = [](const clap_plugin_t* p) { guardedVoid([&] { self(p)->deactivate(); }); };
    clapPlugin_.start_processing = [](const clap_plugin_t* p) -> bool { return guarded(false, [&] { return self(p)->startProcessing(); }); };
    clapPlugin_.stop_processing = [](const clap_plugin_t* p) { guardedVoid([&] { self(p)->stopProcessing(); }); };
    clapPlugin_.reset = [](const clap_plugin_t* p) { guardedVoid([&] { self(p)->reset(); }); };
    clapPlugin_.process = [](const clap_plugin_t* p, const clap_process_t* pr) -> clap_process_status {
        return guarded<clap_process_status>(CLAP_PROCESS_CONTINUE, [&] { return self(p)->process(pr); });
    };
    clapPlugin_.get_extension = [](const clap_plugin_t* p, const char* id) -> const void* {
        return guarded<const void*>(nullptr, [&] { return self(p)->getExtension(id); });
    };
    clapPlugin_.on_main_thread = [](const clap_plugin_t* p) { guardedVoid([&] { self(p)->onMainThread(); }); };

    for (uint32_t i = 0; i < TW_PARAM_COUNT; ++i) values_[i].store(twParamInfo(i).def);
}

TearwashClap::~TearwashClap() {
    delete unit_;
    delete next_.exchange(nullptr);
    delete retired_.exchange(nullptr);
}

void TearwashClap::destroy() {
    destroyGuiWindow();
    delete this;
}

void TearwashClap::createGuiWindow() {
    if (!gui_) gui_ = std::make_unique<TwGui>(this);
}

void TearwashClap::destroyGuiWindow() { gui_.reset(); }

// --- Programs ------------------------------------------------------------------------------------

TearwashClap::Unit* TearwashClap::build(int fl, int pg) const {
    // The 225 keeps an engine for the 224XL program of the same index, so switching back is quick.
    const int xlFl = fl == FL_225 ? FL_224XL : fl;
    const int idx = clampProgram(xlFl, pg);
    int n = 0;
    const ProgramInfo& info = xlPrograms(n)[idx];
    auto u = std::make_unique<Unit>();
    u->flavour = fl;
    u->program = clampProgram(fl, pg);
    u->engine.setSampleRate(fs_);
    u->engine.setProgram(makeAlgorithm(info.algorithm));
    XlRegs base = xlFactory(info);
    u->engine.setControls(regsFromValues(base));
    u->engine.setCleanConverters(paramValue(TW_CLEAN) >= 0.5);
    u->pad = std::max(0, latency_ - static_cast<int>(std::lround(u->engine.latency())));
    if (fl == FL_225) u->pad = latency_;
    return u.release();
}

XlRegs TearwashClap::regsFromValues(const XlRegs& base) const {
    XlRegs r = base;
    auto c = [&](uint32_t id) { return static_cast<uint8_t>(toCode(paramValue(id))); };
    r.at(1, 1) = c(TW_BASS); r.at(1, 2) = c(TW_MID); r.at(1, 3) = c(TW_XOVER);
    r.at(1, 4) = c(TW_TREBLE); r.at(1, 5) = c(TW_DEPTH); r.at(1, 6) = c(TW_PREDELAY);
    r.at(3, 3) = c(TW_CHORUS); r.at(3, 4) = c(TW_HFBW); r.at(3, 5) = c(TW_DIFFUSION); r.at(3, 6) = c(TW_DEFINITION);
    r.size = c(TW_SIZE);
    r.options = static_cast<uint8_t>((base.options & ~0xC0) | (paramValue(TW_MODEENH) >= 0.5 ? 0x40 : 0)
                                     | (paramValue(TW_DECAYOPT) >= 0.5 ? 0x80 : 0));
    return r;
}

// Main thread. loadFactory: the program's factory codes replace the code parameters (not for
// the 225, whose three controls carry over).
void TearwashClap::rebuild(bool loadFactory) {
    std::lock_guard<std::mutex> lock(buildMutex_);
    delete retired_.exchange(nullptr);
    const int fl = flavour();
    const int pg = program();
    targetKey_.store(keyOf(fl, pg));
    if (loadFactory && fl != FL_225) {
        int n = 0;
        const XlRegs r = xlFactory(xlPrograms(n)[clampProgram(fl, pg)]);
        const struct { uint32_t id; uint8_t v; } map[] = {
            { TW_BASS, r.at(1, 1) }, { TW_MID, r.at(1, 2) }, { TW_XOVER, r.at(1, 3) }, { TW_TREBLE, r.at(1, 4) },
            { TW_DEPTH, r.at(1, 5) }, { TW_PREDELAY, r.at(1, 6) }, { TW_CHORUS, r.at(3, 3) }, { TW_HFBW, r.at(3, 4) },
            { TW_DIFFUSION, r.at(3, 5) }, { TW_DEFINITION, r.at(3, 6) }, { TW_SIZE, r.size },
        };
        for (const auto& m : map) setValue(m.id, fromCode(m.v));
        setValue(TW_MODEENH, (r.options & 0x40) ? 1.0 : 0.0);
        setValue(TW_DECAYOPT, (r.options & 0x80) ? 1.0 : 0.0);
        setValue(TW_PROGRAM, pg);
        rescanValues_.store(true);
    }
    if (!active_) return;
    Unit* u = build(fl, pg);
    coreRate_.store(u->engine.coreRate());
    delete next_.exchange(u);
}

void TearwashClap::selectProgram(int fl, int pg) {
    fl = std::max(0, std::min(FL_COUNT - 1, fl));
    pg = clampProgram(fl, pg);
    targetKey_.store(keyOf(fl, pg));   // so the audio thread does not ask for it again
    double old[TW_PARAM_COUNT];
    for (uint32_t i = 0; i < TW_PARAM_COUNT; ++i) old[i] = paramValue(i);
    setValue(TW_FLAVOUR, fl);
    setValue(TW_PROGRAM, pg);
    rebuild(true);
    rescanValues_.store(false);   // the gestures below tell the host
    for (uint32_t i = 0; i < TW_PARAM_COUNT; ++i) {
        const double v = paramValue(i);
        if (v == old[i]) continue;
        queueOut({ CLAP_EVENT_PARAM_GESTURE_BEGIN, i, 0.0, CLAP_EVENT_IS_LIVE });
        queueOut({ CLAP_EVENT_PARAM_VALUE, i, v, CLAP_EVENT_IS_LIVE });
        queueOut({ CLAP_EVENT_PARAM_GESTURE_END, i, 0.0, CLAP_EVENT_IS_LIVE });
    }
    requestFlush();
    markDirty();
}

// Audio thread: a host change of flavour or program asks the main thread for a new engine.
void TearwashClap::checkProgramChange() {
    const int key = keyOf(flavour(), program());
    if (key == targetKey_.load() || key == askedKey_) return;
    askedKey_ = key;
    rebuildRequest_.store(true);
    if (host_ && host_->request_callback) host_->request_callback(host_);
}

// Audio thread: register changes of the running program.
void TearwashClap::applyControls() {
    if (!unit_) return;
    Engine& e = unit_->engine;
    const XlRegs r = regsFromValues(e.controls());
    if (std::memcmp(&r, &e.controls(), sizeof(XlRegs)) != 0) e.setControls(r);
    e.setCleanConverters(paramValue(TW_CLEAN) >= 0.5);
}

// --- Lifecycle -----------------------------------------------------------------------------------

bool TearwashClap::activate(double sampleRate, uint32_t, uint32_t maxFrames) {
    fs_ = sampleRate;
    // One latency for every program: the longest loop (109 steps) has the largest (01 §3).
    latency_ = static_cast<int>(std::ceil(Engine::latencyFor(fs_, 109)));
    for (auto& r : ring_) r.assign(kRing, 0.f);
    ringPos_ = 0;
    maxFrames_ = std::max<uint32_t>(maxFrames, 64);
    for (auto& s : scratch_) s.assign(maxFrames_, 0.f);
    plate_.setSampleRate(fs_);
    gainCoef_ = 1.0 - std::exp(-1.0 / (0.010 * fs_));
    delete unit_;
    delete next_.exchange(nullptr);
    delete retired_.exchange(nullptr);
    active_ = true;
    unit_ = build(flavour(), program());
    coreRate_.store(unit_->engine.coreRate());
    targetKey_.store(keyOf(unit_->flavour, unit_->program));
    askedKey_ = -1;
    inGain_ = std::pow(10.0, paramValue(TW_INGAIN) / 20.0);
    mix_ = paramValue(TW_MIX);
    outGain_ = paramValue(TW_OUTGAIN) <= kOutGainFloorDb ? 0.0 : std::pow(10.0, paramValue(TW_OUTGAIN) / 20.0);
    return true;
}

void TearwashClap::reset() {
    if (unit_) unit_->engine.reset();
    plate_.reset();
    for (auto& r : ring_) std::fill(r.begin(), r.end(), 0.f);
}

void TearwashClap::markDirty() {
    stateDirty_.store(true);
    if (host_ && host_->request_callback) host_->request_callback(host_);
}

void TearwashClap::onMainThread() {
    delete retired_.exchange(nullptr);
    if (rebuildRequest_.exchange(false)) rebuild(true);
    if (rescanValues_.exchange(false) && host_) {
        const auto* hp = static_cast<const clap_host_params_t*>(host_->get_extension(host_, CLAP_EXT_PARAMS));
        if (hp && hp->rescan) hp->rescan(host_, CLAP_PARAM_RESCAN_VALUES | CLAP_PARAM_RESCAN_TEXT);
    }
    if (stateDirty_.exchange(false) && host_) {
        const auto* hs = static_cast<const clap_host_state_t*>(host_->get_extension(host_, CLAP_EXT_STATE));
        if (hs && hs->mark_dirty) hs->mark_dirty(host_);
    }
}

// --- Processing ----------------------------------------------------------------------------------

void TearwashClap::handleEvent(const clap_event_header_t* hdr) {
    if (!hdr || hdr->size < sizeof(clap_event_header_t) || hdr->space_id != CLAP_CORE_EVENT_SPACE_ID) return;
    if (hdr->type != CLAP_EVENT_PARAM_VALUE || hdr->size < sizeof(clap_event_param_value_t)) return;
    const auto* ev = reinterpret_cast<const clap_event_param_value_t*>(hdr);
    if (ev->param_id < TW_PARAM_COUNT) setValue(ev->param_id, ev->value);
}

void TearwashClap::render(const float* inL, const float* inR, float* outL, float* outR, uint32_t n) {
    float* xl = scratch_[0].data();
    float* xr = scratch_[1].data();
    float* w[4] = { scratch_[2].data(), scratch_[3].data(), scratch_[4].data(), scratch_[5].data() };
    const double inTarget = std::pow(10.0, paramValue(TW_INGAIN) / 20.0);
    const double og = paramValue(TW_OUTGAIN);
    const double outTarget = og <= kOutGainFloorDb ? 0.0 : std::pow(10.0, og / 20.0);
    const double mixTarget = paramValue(TW_MIX);
    float peak = 0.f;
    double ig = inGain_;
    for (uint32_t i = 0; i < n; ++i) {
        ig += (inTarget - ig) * gainCoef_;
        const float l = inL ? inL[i] : 0.f, r = inR ? inR[i] : l;
        xl[i] = static_cast<float>(l * ig);
        xr[i] = static_cast<float>(r * ig);
        peak = std::max(peak, std::max(std::fabs(xl[i]), std::fabs(xr[i])));
    }
    inGain_ = ig;

    const bool plate = unit_->flavour == FL_225;
    int pad = unit_->pad;
    float* wl;
    float* wr;
    if (plate) {
        std::copy(xl, xl + n, w[0]);
        std::copy(xr, xr + n, w[1]);
        plate_.process(w[0], w[1], static_cast<int>(n), 1.0, paramValue(TW_MID), paramValue(TW_TREBLE), paramValue(TW_PREDELAY));
        for (uint32_t i = 0; i < n; ++i) { w[0][i] -= xl[i]; w[1][i] -= xr[i]; }
        wl = w[0]; wr = w[1];
    } else {
        unit_->engine.process(xl, xr, w, static_cast<int>(n));
        const bool rear = paramValue(TW_REAR) >= 0.5;
        wl = rear ? w[1] : w[0];
        wr = rear ? w[3] : w[2];
    }

    double mx = mix_, go = outGain_;
    const int mask = kRing - 1;
    for (uint32_t i = 0; i < n; ++i) {
        mx += (mixTarget - mx) * gainCoef_;
        go += (outTarget - go) * gainCoef_;
        const int p = ringPos_;
        ring_[0][p] = inL ? inL[i] : 0.f;
        ring_[1][p] = inR ? inR[i] : ring_[0][p];
        ring_[2][p] = wl[i];
        ring_[3][p] = wr[i];
        const int pd = (p - latency_) & mask, pw = (p - pad) & mask;
        const double yl = (1.0 - mx) * ring_[0][pd] + mx * ring_[2][pw];
        const double yr = (1.0 - mx) * ring_[1][pd] + mx * ring_[3][pw];
        if (outL) outL[i] = static_cast<float>(yl * go);
        if (outR) outR[i] = static_cast<float>(yr * go);
        ringPos_ = (p + 1) & mask;
    }
    mix_ = mx;
    outGain_ = go;

    // Headroom meter: five LEDs 6 dB apart up to the converter's full scale (02 §4).
    peakHold_ = std::max(peak, peakHold_ * static_cast<float>(std::exp(-double(n) / (0.3 * fs_))));
    static const float kLed[5] = { 0.056f, 0.112f, 0.224f, 0.448f, 0.89f };
    int leds = 0;
    while (leds < 5 && peakHold_ >= kLed[leds]) ++leds;
    headroom_.store(leds, std::memory_order_relaxed);
    if (peak >= 1.0f) overload_.store(true, std::memory_order_relaxed);
    else if (peakHold_ < 0.5f) overload_.store(false, std::memory_order_relaxed);
}

clap_process_status TearwashClap::process(const clap_process_t* process) {
    if (!process) return CLAP_PROCESS_CONTINUE;
    ScopedFlushDenormals ftz;

    // A new engine from the main thread (the old one goes back to be freed there).
    if (!retired_.load()) {
        if (Unit* u = next_.exchange(nullptr)) {
            retired_.store(unit_);
            unit_ = u;
            if (host_ && host_->request_callback) host_->request_callback(host_);
        }
    }

    const uint32_t frames = process->frames_count;
    const uint32_t numEvents = process->in_events ? process->in_events->size(process->in_events) : 0;
    const bool haveIn = process->audio_inputs && process->audio_inputs_count > 0 && process->audio_inputs[0].data32;
    const bool haveOut = process->audio_outputs && process->audio_outputs_count > 0 && process->audio_outputs[0].data32;
    const float* inL = (haveIn && process->audio_inputs[0].channel_count > 0) ? process->audio_inputs[0].data32[0] : nullptr;
    const float* inR = (haveIn && process->audio_inputs[0].channel_count > 1) ? process->audio_inputs[0].data32[1] : inL;
    float* outL = (haveOut && process->audio_outputs[0].channel_count > 0) ? process->audio_outputs[0].data32[0] : nullptr;
    float* outR = (haveOut && process->audio_outputs[0].channel_count > 1) ? process->audio_outputs[0].data32[1] : nullptr;

    uint32_t ei = 0;
    for (uint32_t frame = 0; frame < frames;) {
        while (ei < numEvents) {
            const clap_event_header_t* hdr = process->in_events->get(process->in_events, ei);
            if (hdr && hdr->time > frame) break;
            handleEvent(hdr);
            ++ei;
        }
        uint32_t next = frames;
        if (ei < numEvents) {
            const clap_event_header_t* hdr = process->in_events->get(process->in_events, ei);
            if (hdr && hdr->time < next) next = hdr->time;
        }
        checkProgramChange();
        applyControls();
        uint32_t todo = next > frame ? next - frame : 0;
        if (todo == 0) { ++ei; continue; }
        while (todo > 0) {
            const uint32_t m = std::min(todo, maxFrames_);
            if (unit_ && maxFrames_ > 0) {
                render(inL ? inL + frame : nullptr, inR ? inR + frame : nullptr, outL ? outL + frame : nullptr,
                       outR ? outR + frame : nullptr, m);
            } else {
                for (uint32_t i = 0; i < m; ++i) {
                    if (outL) outL[frame + i] = inL ? inL[frame + i] : 0.f;
                    if (outR) outR[frame + i] = inR ? inR[frame + i] : 0.f;
                }
            }
            frame += m;
            todo -= m;
        }
    }
    while (ei < numEvents) handleEvent(process->in_events->get(process->in_events, ei++));
    pushOut(process->out_events);
    return CLAP_PROCESS_CONTINUE;
}

const void* TearwashClap::getExtension(const char* id) {
    if (!id) return nullptr;
    if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) return &g_audioPorts;
    if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) return &g_params;
    if (std::strcmp(id, CLAP_EXT_STATE) == 0) return &g_state;
    if (std::strcmp(id, CLAP_EXT_LATENCY) == 0) return &g_latency;
    if (std::strcmp(id, CLAP_EXT_GUI) == 0) return &g_tearwashGuiExtension;
    return nullptr;
}

// --- Params --------------------------------------------------------------------------------------

bool TearwashClap::paramsInfo(uint32_t index, clap_param_info_t* info) const {
    if (index >= TW_PARAM_COUNT || !info) return false;
    std::memset(info, 0, sizeof(*info));
    const TwParamInfo& p = twParamInfo(index);
    info->id = index;
    info->flags = CLAP_PARAM_IS_AUTOMATABLE;
    if (p.kind == TwKind::Choice || p.kind == TwKind::Toggle) info->flags |= CLAP_PARAM_IS_STEPPED;
    if (p.kind == TwKind::Choice) info->flags |= CLAP_PARAM_IS_ENUM;
    std::snprintf(info->name, sizeof(info->name), "%s", p.name);
    std::snprintf(info->module, sizeof(info->module), "%s", p.module);
    info->min_value = p.min;
    info->max_value = p.max;
    info->default_value = p.def;
    return true;
}

bool TearwashClap::paramsValue(clap_id id, double* out) const {
    if (id >= TW_PARAM_COUNT || !out) return false;
    *out = values_[id].load(std::memory_order_relaxed);
    return true;
}

bool TearwashClap::paramsValueToText(clap_id id, double value, char* buf, uint32_t cap) const {
    if (id >= TW_PARAM_COUNT || !buf || cap == 0) return false;
    twValueText(id, value, flavour(), program(), coreRate(), buf, cap);
    return true;
}

bool TearwashClap::paramsTextToValue(clap_id id, const char* text, double* out) const {
    return twTextToValue(id, text, flavour(), out);
}

void TearwashClap::paramsFlush(const clap_input_events_t* in, const clap_output_events_t* out) {
    if (in && in->size && in->get) {
        const uint32_t n = in->size(in);
        for (uint32_t i = 0; i < n; ++i) handleEvent(in->get(in, i));
    }
    // Not processing: register changes reach the engine at the next process call.
    if (active_) checkProgramChange();
    pushOut(out);
}

void TearwashClap::requestFlush() {
    if (!host_) return;
    const auto* hp = static_cast<const clap_host_params_t*>(host_->get_extension(host_, CLAP_EXT_PARAMS));
    if (hp && hp->request_flush) hp->request_flush(host_);
    else if (host_->request_process) host_->request_process(host_);
}

void TearwashClap::queueOut(const OutEvent& ev) {
    std::lock_guard<std::mutex> lock(outMutex_);
    if (outCount_ < kOutCapacity) outQueue_[outCount_++] = ev;
}

void TearwashClap::pushOut(const clap_output_events_t* out) {
    if (!out || !out->try_push) return;
    size_t count = 0;
    {
        std::unique_lock<std::mutex> lock(outMutex_, std::try_to_lock);
        if (!lock.owns_lock()) return;
        count = outCount_;
        std::copy(outQueue_.begin(), outQueue_.begin() + static_cast<long>(count), drain_.begin());
        outCount_ = 0;
    }
    for (size_t i = 0; i < count; ++i) {
        const OutEvent& ev = drain_[i];
        if (ev.type == CLAP_EVENT_PARAM_VALUE) {
            clap_event_param_value_t e{};
            e.header.size = sizeof(e);
            e.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            e.header.type = CLAP_EVENT_PARAM_VALUE;
            e.header.flags = ev.flags;
            e.param_id = ev.paramId;
            e.note_id = -1; e.port_index = -1; e.channel = -1; e.key = -1;
            e.value = ev.value;
            out->try_push(out, &e.header);
        } else {
            clap_event_param_gesture_t e{};
            e.header.size = sizeof(e);
            e.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            e.header.type = ev.type;
            e.header.flags = ev.flags;
            e.param_id = ev.paramId;
            out->try_push(out, &e.header);
        }
    }
}

void TearwashClap::onBeginEditFromGui(clap_id id) {
    if (id >= TW_PARAM_COUNT) return;
    queueOut({ CLAP_EVENT_PARAM_GESTURE_BEGIN, id, 0.0, CLAP_EVENT_IS_LIVE });
    requestFlush();
}

void TearwashClap::onParamValueFromGui(clap_id id, double value) {
    if (id >= TW_PARAM_COUNT) return;
    if (id == TW_FLAVOUR || id == TW_PROGRAM) {
        selectProgram(id == TW_FLAVOUR ? static_cast<int>(twClamp(id, value)) : flavour(),
                      id == TW_PROGRAM ? static_cast<int>(twClamp(id, value)) : program());
        return;
    }
    setValue(id, value);
    queueOut({ CLAP_EVENT_PARAM_VALUE, id, values_[id].load(), CLAP_EVENT_IS_LIVE });
    requestFlush();
    markDirty();
}

void TearwashClap::onEndEditFromGui(clap_id id) {
    if (id >= TW_PARAM_COUNT) return;
    queueOut({ CLAP_EVENT_PARAM_GESTURE_END, id, 0.0, CLAP_EVENT_IS_LIVE });
    requestFlush();
}

// --- State ---------------------------------------------------------------------------------------

std::string TearwashClap::stateText() const {
    std::string out = "tearwash225 1\n";
    char buf[96];
    for (uint32_t i = 0; i < TW_PARAM_COUNT; ++i) {
        std::snprintf(buf, sizeof(buf), "%s=%.17g\n", twParamInfo(i).key, paramValue(i));
        out += buf;
    }
    return out;
}

bool TearwashClap::loadStateText(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || line.compare(0, 11, "tearwash225") != 0) return false;
    double v[TW_PARAM_COUNT];
    for (uint32_t i = 0; i < TW_PARAM_COUNT; ++i) v[i] = twParamInfo(i).def;
    while (std::getline(in, line)) {
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const uint32_t id = twParamByKey(line.substr(0, eq).c_str());
        if (id < TW_PARAM_COUNT) v[id] = std::strtod(line.c_str() + eq + 1, nullptr);
    }
    for (uint32_t i = 0; i < TW_PARAM_COUNT; ++i) setValue(i, v[i]);
    rebuild(false);
    return true;
}

bool TearwashClap::stateSave(const clap_ostream_t* stream) {
    if (!stream || !stream->write) return false;
    const std::string s = stateText();
    size_t done = 0;
    while (done < s.size()) {
        const int64_t n = stream->write(stream, s.data() + done, s.size() - done);
        if (n <= 0) return false;
        done += static_cast<size_t>(n);
    }
    return true;
}

bool TearwashClap::stateLoad(const clap_istream_t* stream) {
    if (!stream || !stream->read) return false;
    std::string data;
    char chunk[4096];
    while (data.size() < (1u << 16)) {
        const int64_t n = stream->read(stream, chunk, sizeof(chunk));
        if (n < 0) return false;
        if (n == 0) break;
        data.append(chunk, static_cast<size_t>(std::min<int64_t>(n, sizeof(chunk))));
    }
    return loadStateText(data);
}

// --- Entry ---------------------------------------------------------------------------------------

static const char* g_features[] = { CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, CLAP_PLUGIN_FEATURE_REVERB,
                                    CLAP_PLUGIN_FEATURE_STEREO, nullptr };

static const clap_plugin_descriptor_t g_descriptor = {
    CLAP_VERSION,
    "com.holstebroe.tearwash225",
    "Tearwash 225",
    "holstebroe",
    "https://github.com/holstebroe/Tannhauser",
    "",
    "",
    "0.1.0",
    "Digital reverberator modelled on the Lexicon 224 family",
    g_features,
};

static uint32_t factoryCount(const clap_plugin_factory_t*) { return 1; }
static const clap_plugin_descriptor_t* factoryDescriptor(const clap_plugin_factory_t*, uint32_t i) {
    return i == 0 ? &g_descriptor : nullptr;
}
static const clap_plugin_t* factoryCreate(const clap_plugin_factory_t*, const clap_host_t* host, const char* id) {
    if (!host || !id || !clap_version_is_compatible(host->clap_version)) return nullptr;
    if (std::strcmp(id, g_descriptor.id) != 0) return nullptr;
    try {
        auto* p = new TearwashClap(host);
        return p->getClapPlugin();
    } catch (...) {
        return nullptr;
    }
}

static const clap_plugin_factory_t g_factory = { factoryCount, factoryDescriptor, factoryCreate };

static bool entryInit(const char*) { return true; }
static void entryDeinit() {}
static const void* entryGetFactory(const char* id) {
    return (id && std::strcmp(id, CLAP_PLUGIN_FACTORY_ID) == 0) ? &g_factory : nullptr;
}

} // namespace tearwash

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION, tearwash::entryInit, tearwash::entryDeinit, tearwash::entryGetFactory
};
