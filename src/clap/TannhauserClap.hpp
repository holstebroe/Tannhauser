#ifndef TANNHAUSER_CLAP_HPP
#define TANNHAUSER_CLAP_HPP

// CLAP plugin wrapper (adapted from Acidus' AcidusClap): parameters from the
// single Params table, note/MIDI input, text state, host gestures for GUI
// edits and preset loads, and the software preset system (spec 05).

#include <clap/clap.h>
#include "core/SynthEngine.hpp"
#include "presets/Presets.hpp"
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace tannhauser {

struct OutParamEvent {
    uint16_t type;   // CLAP_EVENT_PARAM_GESTURE_BEGIN / VALUE / GESTURE_END
    clap_id paramId;
    double value;
    uint32_t flags;
};

struct GuiNoteEvent { int key; double velocity; bool on; };

class TannhauserClap {
public:
    explicit TannhauserClap(const clap_host_t* host);
    ~TannhauserClap() = default;

    const clap_plugin_t* getClapPlugin() const { return &clapPlugin_; }

    bool init() { return true; }
    void destroy();
    bool activate(double sampleRate, uint32_t minFrames, uint32_t maxFrames);
    void deactivate() {}
    bool startProcessing() { return true; }
    void stopProcessing() {}
    void reset();
    clap_process_status process(const clap_process_t* process);
    const void* getExtension(const char* id);
    void onMainThread();

    // Params.
    uint32_t paramsCount() const { return PARAM_COUNT; }
    bool paramsInfo(uint32_t index, clap_param_info_t* info) const;
    bool paramsValue(clap_id id, double* out) const;
    double paramValue(clap_id id) const { return id < PARAM_COUNT ? values_[id].load(std::memory_order_relaxed) : 0.0; }
    bool paramsValueToText(clap_id id, double value, char* buf, uint32_t cap) const;
    bool paramsTextToValue(clap_id id, const char* text, double* out) const;
    void paramsFlush(const clap_input_events_t* in, const clap_output_events_t* out);

    // GUI edits (gestures to the host).
    void onBeginEditFromGui(clap_id id);
    void onParamValueFromGui(clap_id id, double value);
    void onEndEditFromGui(clap_id id);
    // GUI keyboard.
    void guiNote(int key, double velocity, bool on);
    bool isKeyDown(int key) const { return key >= 0 && key < 128 && keyDown_[key].load(std::memory_order_relaxed) != 0; }

    // Presets (spec 05).
    PresetLibrary& library() { return library_; }
    void loadPreset(int index);
    void stepPreset(int delta);
    int currentPresetIndex() const { return presetIndex_.load(); }
    std::string currentPresetName() const;
    bool isPresetModified() const;
    void loadFactoryTone(int channel, int button, int targetLine);
    // Memory slots 0..3 (M1..M4: M1/M3 line I, M2/M4 line II).
    void storeMemory(int slot);
    void recallMemory(int slot);
    bool hasMemory(int slot) const;
    void initPatch();
    bool saveUserPreset(const std::string& path, std::string& error);
    ParamValues currentValues() const;

    // State.
    bool stateSave(const clap_ostream_t* stream);
    bool stateLoad(const clap_istream_t* stream);
    std::string stateText() const;
    bool loadStateText(const std::string& text);

    SynthEngine& engine() { return engine_; }
    const clap_host_t* host() const { return host_; }
    class GuiWindow* getGuiWindow() { return gui_.get(); }
    void createGuiWindow();
    void destroyGuiWindow();

private:
    const clap_host_t* host_ = nullptr;
    clap_plugin_t clapPlugin_{};
    SynthEngine engine_;
    std::unique_ptr<class GuiWindow> gui_;
    PresetLibrary library_;

    std::atomic<double> values_[PARAM_COUNT];
    std::array<std::atomic<uint8_t>, 128> keyDown_{};

    // The preset values as loaded (for the modified star) and its name.
    mutable std::mutex presetMutex_;
    ParamValues loadedValues_{};
    std::string presetName_{"IN Init"};
    std::atomic<int> presetIndex_{0};
    // Memory slots: line parameters only (LP_COUNT values each).
    std::array<std::array<double, LP_COUNT>, 4> memory_{};
    std::array<bool, 4> memoryValid_{};

    std::atomic<bool> stateDirty_{false};

    // Outgoing param events (GUI thread -> audio thread -> host).
    static constexpr size_t kOutCapacity = 2048;
    std::mutex outMutex_;
    std::array<OutParamEvent, kOutCapacity> outQueue_{};
    size_t outCount_ = 0;
    std::array<OutParamEvent, kOutCapacity> drain_{};
    void queueOut(const OutParamEvent& ev, bool audioThread);
    void pushOut(const clap_output_events_t* out);
    void requestFlush();

    // GUI keyboard notes.
    std::mutex noteMutex_;
    std::array<GuiNoteEvent, 64> noteQueue_{};
    size_t noteCount_ = 0;

    void setValue(clap_id id, double v) { values_[id].store(clampParam(id, v), std::memory_order_relaxed); }
    void applyValues(const ParamValues& v, bool storedOnly, bool notifyHost);
    void handleEvent(const clap_event_header_t* hdr);
    void syncEngineParams();
    void markDirty();
};

} // namespace tannhauser

#endif
