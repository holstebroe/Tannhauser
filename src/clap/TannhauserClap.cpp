#include "TannhauserClap.hpp"
#include "gui/GuiWindow.hpp"
#include <clap/ext/state.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <vector>
#if defined(__SSE__) || defined(_M_X64)
#include <xmmintrin.h>
#define TANNHAUSER_HAVE_SSE 1
#endif

namespace tannhauser {

// A C++ exception must never cross into the host.
template <class R, class F>
static R guarded(R fallback, F&& f) noexcept {
    try { return f(); } catch (...) { return fallback; }
}
template <class F>
static void guardedVoid(F&& f) noexcept {
    try { f(); } catch (...) {}
}

#ifdef TANNHAUSER_HAVE_SSE
struct ScopedFlushDenormals {
    unsigned saved = _mm_getcsr();
    ScopedFlushDenormals() { _mm_setcsr(saved | 0x8040u); }
    ~ScopedFlushDenormals() { _mm_setcsr(saved); }
};
#else
struct ScopedFlushDenormals {};
#endif

static TannhauserClap* self(const clap_plugin_t* p) { return static_cast<TannhauserClap*>(p->plugin_data); }

static const clap_plugin_note_ports_t g_notePorts = {
    [](const clap_plugin_t*, bool isInput) -> uint32_t { return isInput ? 1 : 0; },
    [](const clap_plugin_t*, uint32_t index, bool isInput, clap_note_port_info_t* info) -> bool {
        if (!isInput || index != 0 || !info) return false;
        info->id = 0;
        std::snprintf(info->name, sizeof(info->name), "Note Input");
        info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
        info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
        return true;
    }
};

static const clap_plugin_audio_ports_t g_audioPorts = {
    [](const clap_plugin_t*, bool isInput) -> uint32_t { return isInput ? 0 : 1; },
    [](const clap_plugin_t*, uint32_t index, bool isInput, clap_audio_port_info_t* info) -> bool {
        if (isInput || index != 0 || !info) return false;
        info->id = 0;
        std::snprintf(info->name, sizeof(info->name), "Output");
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

TannhauserClap::TannhauserClap(const clap_host_t* host) : host_(host) {
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

    for (uint32_t i = 0; i < PARAM_COUNT; ++i) values_[i].store(paramInfo(i).def);
    for (auto& k : keyDown_) k.store(0);
    loadedValues_ = defaultValues();
    syncEngineParams();
}

void TannhauserClap::destroy() {
    destroyGuiWindow();
    delete this;
}

void TannhauserClap::createGuiWindow() {
    if (!gui_) gui_ = std::make_unique<GuiWindow>(this);
}

void TannhauserClap::destroyGuiWindow() { gui_.reset(); }

bool TannhauserClap::activate(double sampleRate, uint32_t, uint32_t) {
    engine_.setSampleRate(sampleRate);
    syncEngineParams();
    return true;
}

void TannhauserClap::reset() {
    engine_.reset();
}

void TannhauserClap::syncEngineParams() {
    for (uint32_t i = 0; i < PARAM_COUNT; ++i) {
        const double v = values_[i].load(std::memory_order_relaxed);
        if (v != engine_.getParam(i)) engine_.setParam(i, v);
    }
}

void TannhauserClap::markDirty() {
    stateDirty_.store(true);
    if (host_ && host_->request_callback) host_->request_callback(host_);
}

// --- Events -----------------------------------------------------------------------------

void TannhauserClap::handleEvent(const clap_event_header_t* hdr) {
    if (!hdr || hdr->size < sizeof(clap_event_header_t) || hdr->space_id != CLAP_CORE_EVENT_SPACE_ID) return;
    switch (hdr->type) {
        case CLAP_EVENT_NOTE_ON: {
            if (hdr->size < sizeof(clap_event_note_t)) return;
            const auto* ev = reinterpret_cast<const clap_event_note_t*>(hdr);
            if (ev->key < 0 || ev->key > 127) return;
            engine_.noteOn(ev->key, ev->velocity);
            keyDown_[static_cast<size_t>(ev->key)].store(1);
            break;
        }
        case CLAP_EVENT_NOTE_OFF:
        case CLAP_EVENT_NOTE_CHOKE: {
            if (hdr->size < sizeof(clap_event_note_t)) return;
            const auto* ev = reinterpret_cast<const clap_event_note_t*>(hdr);
            if (ev->key < 0 || ev->key > 127) {
                if (ev->key == -1) engine_.allNotesOff();
                return;
            }
            engine_.noteOff(ev->key);
            keyDown_[static_cast<size_t>(ev->key)].store(0);
            break;
        }
        case CLAP_EVENT_NOTE_EXPRESSION: {
            if (hdr->size < sizeof(clap_event_note_expression_t)) return;
            const auto* ev = reinterpret_cast<const clap_event_note_expression_t*>(hdr);
            if (ev->expression_id == CLAP_NOTE_EXPRESSION_PRESSURE) {
                if (ev->key >= 0 && ev->key < 128) engine_.notePressure(ev->key, ev->value);
                else engine_.channelPressure(ev->value);
            }
            break;
        }
        case CLAP_EVENT_MIDI: {
            if (hdr->size < sizeof(clap_event_midi_t)) return;
            const auto* ev = reinterpret_cast<const clap_event_midi_t*>(hdr);
            const uint8_t status = ev->data[0] & 0xF0, d1 = ev->data[1], d2 = ev->data[2];
            if (d1 > 127 || d2 > 127) return;
            if (status == 0x90 && d2 > 0) {
                engine_.noteOn(d1, d2 / 127.0);
                keyDown_[d1].store(1);
            } else if (status == 0x80 || (status == 0x90 && d2 == 0)) {
                engine_.noteOff(d1);
                keyDown_[d1].store(0);
            } else if (status == 0xA0) {
                engine_.notePressure(d1, d2 / 127.0);
            } else if (status == 0xD0) {
                engine_.channelPressure(d1 / 127.0);
            } else if (status == 0xE0) {
                const int v = (d2 << 7) | d1;
                engine_.pitchBend((v - 8192) / 8192.0);
            } else if (status == 0xB0) {
                clap_id id = CLAP_INVALID_ID;
                double v = d2 / 127.0;
                if (d1 == 1) engine_.modWheel(v);
                else if (d1 == 11) id = P_EXPRESSION;
                else if (d1 == 64) { id = P_SUS_PEDAL; v = d2 >= 64 ? 1.0 : 0.0; }
                else if (d1 == 120) engine_.allSoundOff();
                else if (d1 == 123) engine_.allNotesOff();
                if (id != CLAP_INVALID_ID) {
                    setValue(id, v);
                    engine_.setParam(id, values_[id].load());
                    queueOut({ CLAP_EVENT_PARAM_VALUE, id, values_[id].load(), CLAP_EVENT_DONT_RECORD }, true);
                }
            }
            break;
        }
        case CLAP_EVENT_PARAM_VALUE: {
            if (hdr->size < sizeof(clap_event_param_value_t)) return;
            const auto* ev = reinterpret_cast<const clap_event_param_value_t*>(hdr);
            if (ev->param_id < PARAM_COUNT) {
                setValue(ev->param_id, ev->value);
                engine_.setParam(ev->param_id, values_[ev->param_id].load());
            }
            break;
        }
        default: break;
    }
}

void TannhauserClap::guiNote(int key, double velocity, bool on) {
    if (key < 0 || key > 127) return;
    std::lock_guard<std::mutex> lock(noteMutex_);
    if (noteCount_ < noteQueue_.size()) noteQueue_[noteCount_++] = { key, velocity, on };
    keyDown_[static_cast<size_t>(key)].store(on ? 1 : 0);
    if (host_ && host_->request_process) host_->request_process(host_);
}

clap_process_status TannhauserClap::process(const clap_process_t* process) {
    if (!process) return CLAP_PROCESS_CONTINUE;
    ScopedFlushDenormals ftz;

    // GUI edits reach the engine here (the GUI only writes the atomics).
    syncEngineParams();
    {
        std::unique_lock<std::mutex> lock(noteMutex_, std::try_to_lock);
        if (lock.owns_lock()) {
            for (size_t i = 0; i < noteCount_; ++i) {
                const GuiNoteEvent& e = noteQueue_[i];
                if (e.on) engine_.noteOn(e.key, e.velocity);
                else engine_.noteOff(e.key);
            }
            noteCount_ = 0;
        }
    }

    const uint32_t frames = process->frames_count;
    const uint32_t numEvents = process->in_events ? process->in_events->size(process->in_events) : 0;
    uint32_t ei = 0;
    const bool haveOut = process->audio_outputs && process->audio_outputs_count > 0 && process->audio_outputs[0].data32;
    float* outL = (haveOut && process->audio_outputs[0].channel_count > 0) ? process->audio_outputs[0].data32[0] : nullptr;
    float* outR = (haveOut && process->audio_outputs[0].channel_count > 1) ? process->audio_outputs[0].data32[1] : nullptr;

    for (uint32_t frame = 0; frame < frames;) {
        while (ei < numEvents) {
            const clap_event_header_t* hdr = process->in_events->get(process->in_events, ei);
            if (!hdr) { ++ei; continue; }
            if (hdr->time > frame) break;
            handleEvent(hdr);
            ++ei;
        }
        uint32_t next = frames;
        if (ei < numEvents) {
            const clap_event_header_t* hdr = process->in_events->get(process->in_events, ei);
            if (hdr && hdr->time < next) next = hdr->time;
        }
        const uint32_t todo = next > frame ? next - frame : 0;
        if (todo > 0) {
            engine_.process(outL ? outL + frame : nullptr, outR ? outR + frame : nullptr, static_cast<int>(todo));
            frame += todo;
        } else {
            ++ei;
        }
    }
    // Remaining events at or after the end.
    while (ei < numEvents) handleEvent(process->in_events->get(process->in_events, ei++));
    pushOut(process->out_events);
    return CLAP_PROCESS_CONTINUE;
}

const void* TannhauserClap::getExtension(const char* id) {
    if (!id) return nullptr;
    if (std::strcmp(id, CLAP_EXT_NOTE_PORTS) == 0) return &g_notePorts;
    if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) return &g_audioPorts;
    if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) return &g_params;
    if (std::strcmp(id, CLAP_EXT_STATE) == 0) return &g_state;
    if (std::strcmp(id, CLAP_EXT_GUI) == 0) return &g_tannhauserGuiExtension;
    return nullptr;
}

void TannhauserClap::onMainThread() {
    if (stateDirty_.exchange(false) && host_) {
        const auto* hs = static_cast<const clap_host_state_t*>(host_->get_extension(host_, CLAP_EXT_STATE));
        if (hs && hs->mark_dirty) hs->mark_dirty(host_);
    }
}

// --- Params ---------------------------------------------------------------------------------

bool TannhauserClap::paramsInfo(uint32_t index, clap_param_info_t* info) const {
    if (index >= PARAM_COUNT || !info) return false;
    std::memset(info, 0, sizeof(*info));
    const ParamInfo& p = paramInfo(index);
    info->id = index;
    info->flags = CLAP_PARAM_IS_AUTOMATABLE;
    if (p.flags & PF_STEPPED) info->flags |= CLAP_PARAM_IS_STEPPED;
    std::snprintf(info->name, sizeof(info->name), "%s", p.name);
    std::snprintf(info->module, sizeof(info->module), "%s", p.module);
    info->min_value = p.min;
    info->max_value = p.max;
    info->default_value = p.def;
    return true;
}

bool TannhauserClap::paramsValue(clap_id id, double* out) const {
    if (id >= PARAM_COUNT || !out) return false;
    *out = values_[id].load(std::memory_order_relaxed);
    return true;
}

bool TannhauserClap::paramsValueToText(clap_id id, double value, char* buf, uint32_t cap) const {
    if (id >= PARAM_COUNT || !buf || cap == 0) return false;
    paramValueText(id, value, buf, cap, values_[P_ENV_LONG].load(std::memory_order_relaxed) >= 0.5);
    return true;
}

bool TannhauserClap::paramsTextToValue(clap_id id, const char* text, double* out) const {
    return paramTextToValue(id, text, out, values_[P_ENV_LONG].load(std::memory_order_relaxed) >= 0.5);
}

void TannhauserClap::paramsFlush(const clap_input_events_t* in, const clap_output_events_t* out) {
    if (in && in->size && in->get) {
        const uint32_t n = in->size(in);
        for (uint32_t i = 0; i < n; ++i) handleEvent(in->get(in, i));
    }
    pushOut(out);
}

void TannhauserClap::requestFlush() {
    if (!host_) return;
    const auto* hp = static_cast<const clap_host_params_t*>(host_->get_extension(host_, CLAP_EXT_PARAMS));
    if (hp && hp->request_flush) hp->request_flush(host_);
    else if (host_->request_process) host_->request_process(host_);
}

void TannhauserClap::queueOut(const OutParamEvent& ev, bool audioThread) {
    std::unique_lock<std::mutex> lock(outMutex_, std::defer_lock);
    if (audioThread) { if (!lock.try_lock()) return; }
    else lock.lock();
    if (outCount_ < kOutCapacity) outQueue_[outCount_++] = ev;
}

void TannhauserClap::pushOut(const clap_output_events_t* out) {
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
        const OutParamEvent& ev = drain_[i];
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

void TannhauserClap::onBeginEditFromGui(clap_id id) {
    if (id >= PARAM_COUNT) return;
    queueOut({ CLAP_EVENT_PARAM_GESTURE_BEGIN, id, 0.0, CLAP_EVENT_IS_LIVE }, false);
    requestFlush();
}

void TannhauserClap::onParamValueFromGui(clap_id id, double value) {
    if (id >= PARAM_COUNT) return;
    setValue(id, value);
    queueOut({ CLAP_EVENT_PARAM_VALUE, id, values_[id].load(), CLAP_EVENT_IS_LIVE }, false);
    requestFlush();
}

void TannhauserClap::onEndEditFromGui(clap_id id) {
    if (id >= PARAM_COUNT) return;
    queueOut({ CLAP_EVENT_PARAM_GESTURE_END, id, 0.0, CLAP_EVENT_IS_LIVE }, false);
    requestFlush();
}

// --- Presets ----------------------------------------------------------------------------------

ParamValues TannhauserClap::currentValues() const {
    ParamValues v{};
    for (uint32_t i = 0; i < PARAM_COUNT; ++i) v[i] = values_[i].load(std::memory_order_relaxed);
    return v;
}

void TannhauserClap::applyValues(const ParamValues& v, bool storedOnly, bool notifyHost) {
    for (uint32_t i = 0; i < PARAM_COUNT; ++i) {
        if (storedOnly && !(paramInfo(i).flags & PF_STORED)) continue;
        const double nv = clampParam(i, v[i]);
        if (nv == values_[i].load()) continue;
        values_[i].store(nv);
        if (notifyHost) {
            queueOut({ CLAP_EVENT_PARAM_GESTURE_BEGIN, i, 0.0, CLAP_EVENT_IS_LIVE }, false);
            queueOut({ CLAP_EVENT_PARAM_VALUE, i, nv, CLAP_EVENT_IS_LIVE }, false);
            queueOut({ CLAP_EVENT_PARAM_GESTURE_END, i, 0.0, CLAP_EVENT_IS_LIVE }, false);
        }
    }
    if (notifyHost) {
        requestFlush();
        markDirty();
    }
}

void TannhauserClap::loadPreset(int index) {
    if (index < 0 || index >= library_.count()) return;
    const Preset& p = library_.presets()[static_cast<size_t>(index)];
    applyValues(p.values, true, true);
    std::lock_guard<std::mutex> lock(presetMutex_);
    loadedValues_ = p.values;
    presetName_ = p.name;
    presetIndex_.store(index);
}

void TannhauserClap::stepPreset(int delta) {
    const int n = library_.count();
    if (n <= 0) return;
    int i = presetIndex_.load();
    if (i < 0) i = 0;
    loadPreset(((i + delta) % n + n) % n);
}

std::string TannhauserClap::currentPresetName() const {
    std::lock_guard<std::mutex> lock(presetMutex_);
    return presetName_;
}

bool TannhauserClap::isPresetModified() const {
    std::lock_guard<std::mutex> lock(presetMutex_);
    for (uint32_t i = 0; i < PARAM_COUNT; ++i) {
        if (!(paramInfo(i).flags & PF_STORED)) continue;
        if (std::fabs(values_[i].load() - loadedValues_[i]) > 1e-6) return true;
    }
    return false;
}

void TannhauserClap::loadFactoryTone(int channel, int button, int targetLine) {
    if (channel < 0 || channel > 1 || button < 0 || button > 10 || targetLine < 0 || targetLine > 1) return;
    const int idx = kFactoryToneIndex[channel][button];
    if (idx < 0 || idx >= library_.count()) return;
    const Preset& p = library_.presets()[static_cast<size_t>(idx)];
    ParamValues v = currentValues();
    for (uint32_t k = 0; k < LP_COUNT; ++k) {
        v[lineParam(targetLine, static_cast<LineParam>(k))] = p.values[lineParam(channel, static_cast<LineParam>(k))];
    }
    applyValues(v, true, true);
}

void TannhauserClap::storeMemory(int slot) {
    if (slot < 0 || slot > 3) return;
    const int line = slot % 2;   // M1/M3 line I, M2/M4 line II
    for (uint32_t k = 0; k < LP_COUNT; ++k) memory_[slot][k] = values_[lineParam(line, static_cast<LineParam>(k))].load();
    memoryValid_[slot] = true;
    markDirty();
}

void TannhauserClap::recallMemory(int slot) {
    if (slot < 0 || slot > 3 || !memoryValid_[slot]) return;
    const int line = slot % 2;
    ParamValues v = currentValues();
    for (uint32_t k = 0; k < LP_COUNT; ++k) v[lineParam(line, static_cast<LineParam>(k))] = memory_[slot][k];
    applyValues(v, true, true);
}

bool TannhauserClap::hasMemory(int slot) const { return slot >= 0 && slot < 4 && memoryValid_[slot]; }

void TannhauserClap::initPatch() {
    const int i = library_.indexOf("IN Init");
    if (i >= 0) loadPreset(i);
}

bool TannhauserClap::saveUserPreset(const std::string& path, std::string& error) {
    std::string name;
    const ParamValues v = currentValues();
    if (!library_.saveUserPreset(path, v, name, error)) return false;
    std::lock_guard<std::mutex> lock(presetMutex_);
    loadedValues_ = v;
    presetName_ = name;
    presetIndex_.store(library_.indexOf(name));
    return true;
}

// --- State -------------------------------------------------------------------------------------

std::string TannhauserClap::stateText() const {
    std::string out = patchToText(currentValues(), currentPresetName(), false);
    char buf[64];
    for (int s = 0; s < 4; ++s) {
        if (!memoryValid_[s]) continue;
        out += "mem" + std::to_string(s + 1) + "=";
        for (uint32_t k = 0; k < LP_COUNT; ++k) {
            std::snprintf(buf, sizeof(buf), "%s%.6g", k ? "," : "", memory_[s][k]);
            out += buf;
        }
        out += "\n";
    }
    return out;
}

bool TannhauserClap::loadStateText(const std::string& text) {
    ParamValues v = defaultValues();
    std::string name;
    if (!parsePatchText(text, v, &name)) return false;
    // Memories.
    std::istringstream in(text);
    std::string line;
    memoryValid_ = {};
    while (std::getline(in, line)) {
        if (line.size() > 5 && line.compare(0, 3, "mem") == 0 && line[4] == '=') {
            const int s = line[3] - '1';
            if (s < 0 || s > 3) continue;
            std::istringstream vals(line.substr(5));
            std::string tok;
            uint32_t k = 0;
            while (std::getline(vals, tok, ',') && k < LP_COUNT) {
                const double d = std::strtod(tok.c_str(), nullptr);
                memory_[s][k] = clampParam(lineParam(s % 2, static_cast<LineParam>(k)), d);
                ++k;
            }
            memoryValid_[s] = (k == LP_COUNT);
        }
    }
    for (uint32_t i = 0; i < PARAM_COUNT; ++i) values_[i].store(clampParam(i, v[i]));
    std::lock_guard<std::mutex> lock(presetMutex_);
    presetName_ = name.empty() ? "IN Init" : name;
    const int idx = library_.indexOf(presetName_);
    presetIndex_.store(idx);
    loadedValues_ = idx >= 0 ? library_.presets()[static_cast<size_t>(idx)].values : v;
    return true;
}

bool TannhauserClap::stateSave(const clap_ostream_t* stream) {
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

bool TannhauserClap::stateLoad(const clap_istream_t* stream) {
    if (!stream || !stream->read) return false;
    std::string data;
    char chunk[4096];
    while (data.size() < (1u << 20)) {
        const int64_t n = stream->read(stream, chunk, sizeof(chunk));
        if (n < 0) return false;
        if (n == 0) break;
        data.append(chunk, static_cast<size_t>(std::min<int64_t>(n, sizeof(chunk))));
    }
    return loadStateText(data);
}

// --- Entry ----------------------------------------------------------------------------------------

static const char* g_features[] = { CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_SYNTHESIZER,
                                    CLAP_PLUGIN_FEATURE_STEREO, nullptr };

static const clap_plugin_descriptor_t g_descriptor = {
    CLAP_VERSION,
    "com.holstebroe.tannhauser",
    "Tannhauser",
    "holstebroe",
    "https://github.com/holstebroe/Tannhauser",
    "",
    "",
    "0.1.0",
    "Yamaha CS-80 inspired polyphonic synthesizer",
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
        auto* p = new TannhauserClap(host);
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

} // namespace tannhauser

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION, tannhauser::entryInit, tannhauser::entryDeinit, tannhauser::entryGetFactory
};
