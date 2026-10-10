#ifndef TEARWASH_CLAP_HPP
#define TEARWASH_CLAP_HPP

// Tearwash 225 CLAP plugin (docs/tearwash/01 §5, PLAN TW6): stereo audio effect around the
// tearwash::Engine (224X/XL networks) and the Tannhäuser plate (the 225 flavour). Parameters
// from TwParams, text state, host gestures for GUI edits.
//
// Threads: a program or flavour change builds a new engine on the main thread and hands it to
// the audio thread through an atomic slot; the replaced one comes back through a second slot and
// is freed on the main thread. Register changes of the running program are applied on the audio
// thread (Engine::setControls does not allocate).

#include <clap/clap.h>
#include "TwParams.hpp"
#include "tearwash/engine/Tearwash.hpp"
#include "core/Effects.hpp"

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace tearwash {

class TwGui;

class TearwashClap {
public:
    explicit TearwashClap(const clap_host_t* host);
    ~TearwashClap();

    const clap_plugin_t* getClapPlugin() const { return &clapPlugin_; }

    bool init() { return true; }
    void destroy();
    bool activate(double sampleRate, uint32_t minFrames, uint32_t maxFrames);
    void deactivate() { active_ = false; }
    bool startProcessing() { return true; }
    void stopProcessing() {}
    void reset();
    clap_process_status process(const clap_process_t* process);
    const void* getExtension(const char* id);
    void onMainThread();

    // Params.
    uint32_t paramsCount() const { return TW_PARAM_COUNT; }
    bool paramsInfo(uint32_t index, clap_param_info_t* info) const;
    bool paramsValue(clap_id id, double* out) const;
    double paramValue(clap_id id) const { return id < TW_PARAM_COUNT ? values_[id].load(std::memory_order_relaxed) : 0.0; }
    bool paramsValueToText(clap_id id, double value, char* buf, uint32_t cap) const;
    bool paramsTextToValue(clap_id id, const char* text, double* out) const;
    void paramsFlush(const clap_input_events_t* in, const clap_output_events_t* out);
    uint32_t latency() const { return static_cast<uint32_t>(latency_); }

    // GUI edits (gestures to the host).
    void onBeginEditFromGui(clap_id id);
    void onParamValueFromGui(clap_id id, double value);
    void onEndEditFromGui(clap_id id);
    // Selects a flavour and program from the GUI (main thread): loads the program's factory
    // codes (03 §5) and tells the host about every changed parameter.
    void selectProgram(int flavour, int program);

    int flavour() const { return static_cast<int>(paramValue(TW_FLAVOUR)); }
    int program() const { return clampProgram(flavour(), static_cast<int>(paramValue(TW_PROGRAM))); }
    double coreRate() const { return coreRate_.load(std::memory_order_relaxed); }
    // Input level for the headroom meter: the converter's gain-range code, 0 (low) … 4 (full).
    int headroomLeds() const { return headroom_.load(std::memory_order_relaxed); }
    bool overload() const { return overload_.load(std::memory_order_relaxed); }

    // State.
    bool stateSave(const clap_ostream_t* stream);
    bool stateLoad(const clap_istream_t* stream);
    std::string stateText() const;
    bool loadStateText(const std::string& text);

    const clap_host_t* host() const { return host_; }
    TwGui* getGuiWindow() { return gui_.get(); }
    void createGuiWindow();
    void destroyGuiWindow();

private:
    const clap_host_t* host_ = nullptr;
    clap_plugin_t clapPlugin_{};
    std::unique_ptr<TwGui> gui_;
    std::atomic<double> values_[TW_PARAM_COUNT];

    // A built program: the engine and what it was built for.
    struct Unit {
        Engine engine;
        int flavour = FL_224XL, program = 0;
        int pad = 0;   // host samples that bring the engine's latency up to the plugin's
    };
    Unit* unit_ = nullptr;                       // audio thread
    std::atomic<Unit*> next_{ nullptr };         // main → audio
    std::atomic<Unit*> retired_{ nullptr };      // audio → main
    std::atomic<int> targetKey_{ -1 };           // flavour·64 + program last built or building
    int askedKey_ = -1;                          // audio thread: last change it asked for
    std::atomic<bool> rebuildRequest_{ false };
    std::mutex buildMutex_;
    void rebuild(bool loadFactory);
    Unit* build(int flavour, int program) const;
    static int keyOf(int flavour, int program) { return flavour * 64 + program; }
    XlRegs regsFromValues(const XlRegs& base) const;
    void applyControls();

    tannhauser::PlateReverb plate_;
    bool active_ = false;
    double fs_ = 48000.0;
    int latency_ = 0;
    std::atomic<double> coreRate_{ 32507.94 };

    // Dry and wet alignment delays (plugin latency) and scratch buffers, sized in activate().
    static constexpr int kRing = 1024;
    std::vector<float> ring_[4];   // dry L, dry R, wet L, wet R
    int ringPos_ = 0;
    std::vector<float> scratch_[6];
    uint32_t maxFrames_ = 0;
    double inGain_ = 1.0, outGain_ = 1.0, mix_ = 0.35;
    double gainCoef_ = 0.001;
    std::atomic<int> headroom_{ 0 };
    std::atomic<bool> overload_{ false };
    float peakHold_ = 0.f;
    void render(const float* inL, const float* inR, float* outL, float* outR, uint32_t n);

    std::atomic<bool> stateDirty_{ false };
    std::atomic<bool> rescanValues_{ false };
    void markDirty();

    // Outgoing param events (GUI thread → host).
    struct OutEvent { uint16_t type; clap_id paramId; double value; uint32_t flags; };
    static constexpr size_t kOutCapacity = 512;
    std::mutex outMutex_;
    std::array<OutEvent, kOutCapacity> outQueue_{};
    size_t outCount_ = 0;
    std::array<OutEvent, kOutCapacity> drain_{};
    void queueOut(const OutEvent& ev);
    void pushOut(const clap_output_events_t* out);
    void requestFlush();

    void setValue(clap_id id, double v) { values_[id].store(twClamp(id, v), std::memory_order_relaxed); }
    void handleEvent(const clap_event_header_t* hdr);
    void checkProgramChange();
};

} // namespace tearwash

#endif
