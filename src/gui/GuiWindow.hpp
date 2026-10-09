#ifndef TANNHAUSER_GUI_WINDOW_HPP
#define TANNHAUSER_GUI_WINDOW_HPP

// The plugin window (spec 06): renders the panel into a 2x supersampled
// buffer in layers (static panel, controls, overlays), handles mouse input,
// the preset menu, and the X11 / Win32 window glue (adapted from Acidus).

#include <clap/clap.h>
#include <clap/ext/gui.h>
#include "PanelLayout.hpp"
#include "PanelRenderer.hpp"
#include "FileDialog.hpp"
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace tannhauser {

class TannhauserClap;
class Graphics;

extern const clap_plugin_gui_t g_tannhauserGuiExtension;

class GuiWindow {
public:
    static constexpr uint32_t kDefaultWidth = PanelLayout::kWidth;
    static constexpr uint32_t kDefaultHeight = PanelLayout::kHeight;

    explicit GuiWindow(TannhauserClap* plugin);
    ~GuiWindow();

    bool setParent(const clap_window_t* window);
    bool setSize(uint32_t width, uint32_t height);
    bool show();
    bool hide();
    void destroy();
    bool isVisible() const { return visible_; }

    // Polls the plugin and repaints what changed.
    void renderFrame();
    const std::vector<uint32_t>& getPixelBuffer() const { return pixels_; }
    uint32_t getWidth() const { return width_; }
    uint32_t getHeight() const { return height_; }

    // Input (logical pixels). Exposed for tests.
    void handleMouseDown(int x, int y, bool shift);
    void handleMouseDrag(int x, int y, bool shift);
    void handleMouseUp();
    void handleMouseMove(int x, int y);
    void handleWheel(int x, int y, int delta);
    void handleKey(int key);   // 27 = Escape

    // Menu (tests).
    bool isMenuOpen() const { return menuOpen_; }
    int menuItemCount(int level) const;
    std::string menuItemLabel(int level, int i) const;
    bool menuItemCenter(int level, int i, int& x, int& y) const;
    std::string readoutText() const { return readout_; }
    int controlIndexAt(int x, int y) const;

    // Save As needs a file dialog, run by the platform layer.
    enum class FileRequest { NoRequest, SavePreset };
    FileRequest takeFileRequest();
    FileDialogOptions fileDialogOptions() const;
    void finishFileRequest(const std::string& path);
    void showStatus(const std::string& text);

private:
    TannhauserClap* plugin_;
    uint32_t width_ = kDefaultWidth, height_ = kDefaultHeight;
    mutable std::recursive_mutex mutex_;

    std::vector<uint32_t> static_;    // 2x: panel background and print
    std::vector<uint32_t> compose_;   // 2x: static + controls
    std::vector<uint32_t> display_;   // 2x: compose + overlays (menu, readout)
    std::vector<uint32_t> pixels_;    // 1x output
    bool staticValid_ = false;
    std::vector<CtlState> drawn_;
    std::vector<bool> drawnValid_;
    int dirtyX0_ = 0, dirtyY0_ = 0, dirtyX1_ = 0, dirtyY1_ = 0;
    void addDirty(int x, int y, int w, int h);

    // Interaction.
    int hover_ = -1;
    int active_ = -1;
    int dragStartY_ = 0, dragStartX_ = 0;
    double dragStartValue_ = 0.0;
    bool lastShift_ = false;
    int lastClick_ = -1;
    std::chrono::steady_clock::time_point lastClickTime_{};
    int litTone_[2] = { 13, 13 };
    double ribbonTouch_ = -1.0;
    int keyboardKey_ = -1;
    bool keysDown_[128] = {};

    // Readout / status.
    std::string readout_;
    std::string drawnReadout_;
    std::string status_;
    std::chrono::steady_clock::time_point statusUntil_{};

    // Menu: level 0 = main, level 1 = the hovered category's presets.
    struct MenuItem { std::string label; int action; std::string category; };
    static constexpr int kActInit = -1, kActSave = -2, kActRescan = -3, kActSeparator = -4, kActCategory = -5;
    std::vector<MenuItem> menu_[2];
    bool menuOpen_ = false;
    bool menuWasOpen_ = false;
    int prevMenu_[2][4]{};
    int menuX_[2]{}, menuY_[2]{}, menuW_[2]{}, menuCols_[2]{ 1, 1 }, menuRows_[2]{};
    int menuHover_[2]{ -1, -1 };
    int openCategory_ = -1;
    static constexpr int kItemH = 13;
    void openMenu(int x, int y);
    void closeMenu();
    void openSubmenu(int item);
    int menuHit(int level, int x, int y) const;
    void itemRect(int level, int i, int& x, int& y, int& w, int& h) const;
    void drawMenu(Graphics& g);
    void menuBounds(int level, int& x, int& y, int& w, int& h) const;
    FileRequest pendingFile_ = FileRequest::NoRequest;

    // Visible.
    std::atomic<bool> visible_{ true };
    std::atomic<bool> running_{ false };
    std::thread eventThread_;

    CtlState stateFor(int index) const;
    void composeControl(int index, const CtlState& st);
    void downsample(int x0, int y0, int x1, int y1);
    void setParamFromGui(int param, double value, bool gesture);
    void beginEdit(int index);
    double paramNorm(int param) const;
    void updateReadout();
    int keyAt(int x, int y) const;

#if defined(__linux__) && !defined(__APPLE__)
    void* display11_ = nullptr;
    unsigned long window11_ = 0;
    unsigned long parent11_ = 0;
    bool created11_ = false;
    FileDialogProcess fileDialog_;
    void initX11();
    void drawX11();
    void eventLoopX11();
#elif defined(_WIN32)
    void* hwnd_ = nullptr;
    void* parentHwnd_ = nullptr;
public:
    void initWin32();
    void drawWin32();
private:
#endif
};

} // namespace tannhauser

#endif
