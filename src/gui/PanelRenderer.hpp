#ifndef TANNHAUSER_PANEL_RENDERER_HPP
#define TANNHAUSER_PANEL_RENDERER_HPP

// Procedural drawing of the CS-80-style panel (spec 06 §1), shaded at the
// GUI's 2x supersampled resolution with the Acidus ModernDraw helpers.

#include "PanelLayout.hpp"
#include "Graphics.hpp"
#include <string>

namespace tannhauser {

struct CtlState {
    double norm = 0.0;        // value mapped to 0..1 over the parameter range
    bool on = false;          // rockers, lit tone buttons
    bool hover = false;
    bool active = false;      // being dragged / pressed
    int step = 0;             // levers
    std::string text;         // LCD
    bool modified = false;    // LCD star
    double ribbonTouch = -1;  // ribbon: touch position 0..1, or -1
    double ribbonValue = 0;   // ribbon parameter -1..1
    const bool* keys = nullptr;   // keyboard: 128 key states
    bool hasMemory = false;   // memory tone buttons
};

namespace panel {

void drawStatic(Graphics& g, const PanelLayout& layout);
void drawControl(Graphics& g, const Ctl& c, const CtlState& s);
// Header value readout (right side of the header).
void drawReadout(Graphics& g, const std::string& text);
// Region (logical px) a control repaints, including shadows.
void controlBounds(const Ctl& c, int& x, int& y, int& w, int& h);
void readoutBounds(int& x, int& y, int& w, int& h);
// Width of a readout line (first = the name/value line) and of a control name, and
// the room a control's name has (spec 06: names are short, one size).
float readoutWidth(const std::string& line, bool first);
float nameWidth(const char* text);
float nameRoom(const Ctl& c);
// Tone selector button names (two lines).
const char* toneButtonTop(int row, int button);
const char* toneButtonBottom(int row, int button);

} // namespace panel
} // namespace tannhauser

#endif
