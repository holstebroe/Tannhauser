# 05 — Presets and the preset menu

The hardware stores 22 fixed tones (11 per line) on resistor matrices T51–T54 plus 4
single-line memories, selected by 2 × 14 tone buttons [S §4.4, §13]. The plugin replaces the
tone selector with a **software preset system** [A]: a preset is a full *patch* (both lines and
every parameter flagged S in doc 04), browsed through a categorised menu.

## 1. Patch model

- A patch = name + values for all S-flagged parameters. Unlisted parameters take the
  **Init** patch defaults (doc 04 defaults).
- Text format (`.tpreset`, also the CLAP state body):
  ```
  # Tannhauser preset v1
  name=PD Blade Pad
  l1.saw=1
  l1.lpf=0.62
  ...
  ```
  Unknown keys are ignored, missing keys take defaults; values are clamped to their ranges.
- Recall is a **step** (no glide), as on the hardware [S]. Performance (P) parameters are never
  changed by a preset.

## 2. Naming and categories

Every preset name starts with a two-letter category code and a space:

| Code | Category | Code | Category |
| --- | --- | --- | --- |
| FT | Factory tone (decoded hardware, one line) | PD | Pad |
| FC | Factory combo (decoded hardware, both lines) | ST | Strings |
| BR | Brass | LD | Lead |
| BS | Bass | KY | Keys (pianos, clav, harpsichord) |
| OR | Organ | PL | Pluck / mallet |
| BL | Bell / metallic | FX | Effects / sci-fi |
| SQ | Sequence / arp-friendly | IN | Init / templates |

The menu groups presets by code in this order: IN, FT, FC, PD, ST, BR, LD, BS, KY, OR, PL,
BL, SQ, FX, then **User** (files found on disk).

## 3. Factory decode (FT / FC) — `tools/gen_presets.py`

Source: `docs/reference/cs80_presets.json` (row volts per tone, decoded from the resistor
matrices: `V = 10 V × 10k/(R + 10k)`, J = 10 V, blank = 0 V) [I §14.5].

Row → parameter, with the inversions of doc 02 §9:

| Row | Param | Conversion | Row | Param | Conversion |
| --- | --- | --- | --- | --- | --- |
| 1 | pwmSpeed | V/10 | 14 | fegD | 1 − V/10 |
| 2 | pwmDepth | V/10 | 15 | fegR | 1 − V/10 |
| 3 | pw | V/10 | 16 | vcfLevel | V/10 |
| 4 | square | V ≥ 5 | 17 | sine | V/10 |
| 5 | saw | V ≥ 5 | 18 | vegA | 1 − V/10 |
| 6 | noise | V/10 | 19 | vegD | 1 − V/10 |
| 7 | hpf | V/10 | 20 | vegS | V/10 |
| 8 | resH | 1 − V/10 | 21 | vegR | 1 − V/10 |
| 9 | lpf | V/10 | 22 | level | V/10 |
| 10 | resL | 1 − V/10 | 23 | initBrill | V/10 |
| 11 | il | V/10 | 24 | initLevel | V/10 |
| 12 | al | V/10 | 25 | afterBrill | V/10 |
| 13 | fegA | 1 − V/10 | 26 | afterLevel | V/10 |

Feet are not part of the tone matrices (the Feet lever is a panel control); factory tones use
8′. Global sections (sub-osc, ring mod, chorus…) are at Init values with chorus off.

- **FT** (22 presets): one hardware tone on line I (channel I tones) or line II (channel II
  tones); the other line is silent (`level = 0`), Mix at the centre. Name: `FT String 1` …
- **FC** (11 presets): the two tones sharing a button position (bus) on both lines, e.g.
  `FC String 1+2`, `FC Brass 1+2`, with Detune 0.2. These are the classic way the CS-80 was
  played (both channels on).

Caveats carried from the source: bus-to-button order assumed [I]; channel II read from the
circuit pages only; single cells ±10 %.

**Time-row polarity test** (`python3 tools/analyze_factory_polarity.py`, 2026-10-09). With no
recording available, each hypothesis is scored against plausible time ranges per tone family
(harpsichord: attack ≤ 10 ms, release ≤ 350 ms; strings: attack ≥ 30 ms, release ≥ 250 ms; …,
ranges [D] in the script). Result over the 74 audible time settings of the 22 tones:
"higher V = shorter time" 62/74 plausible, "higher V = longer" 33/74; no single row prefers
the opposite polarity (mixed wiring explains nothing more). The misfits are the four String
tones (fast attack 3–50 ms, release 40–90 ms — consistent with the staccato string vamps the
factory strings are known for) and the 10 ms release of Brass 1/2 and Electric Piano. A
bus-to-button reordering test (exact assignment search) improves the fit by under one tone
per channel, so the printed order (P-9) is kept. Decision: keep the inverted time rows.

Factory tones keep their decoded line levels; loudness is evened out by Patch Gain only (§4).

## 4. Library presets (PD, ST, BR, …)

Hand-designed in `tools/preset_library.py` as parameter dictionaries over the Init patch, aiming
at well-known CS-80 idioms (Blade Runner brass and pads, Vangelis aftertouch swells, "Memories
of Green" glides, Toto-style brass stabs, Wonderful Christmastime sub-osc stabs, sync-free
CS-80 leads with ribbon, ring-mod bells, S&H effects). Target ≥ 60 presets. Each preset must
pass the robustness test (finite output, peak < 0 dBFS for an 8-note chord at velocity 1).

The generator writes `src/presets/PresetData.cpp` (an array of `{name, {key, value}…}`), so
presets need no files at runtime. Run: `python3 tools/gen_presets.py`.

**Loudness normalisation.** Every built-in preset (FT, FC and library) is trimmed with Patch
Gain [A] to **−18 LUFS** (`kLoudnessTarget`, about the library's median before trimming):
ITU-R BS.1770 K-weighted *maximum momentary* loudness (400 ms windows), averaged in LU over two
phrases — a held six-note chord and a twelve-note line, velocity 0.75, no aftertouch
(`src/tests/Loudness.hpp`). Workflow after adding or editing presets:
`python3 tools/gen_presets.py` → build → `build/tannhauser_loudness --fit tools/preset_gains.json`
→ `python3 tools/gen_presets.py` → build. Test T16 fails when any preset is off by ≥ 1 LU.
Before trimming the presets spread over 21.5 LU (−35.2 FT Harpsichord 2 … −13.6 OR Church
Organ); fitted gains are −4.6…+17.3 dB, and after trimming −18.15…−17.89 LUFS. Presets that
rely on aftertouch (e.g. PD Aftertouch Swell) are louder than the target when pressed.

## 5. User presets

- Folder: `$HOME/Documents/Tannhauser/Presets` (Windows: `%USERPROFILE%\Documents\...`),
  created on first save. Every `*.tpreset` there is listed under **User** (re-scanned when the
  menu opens).
- Menu items: **Save As…** (file dialog; name taken from the file name), **Init patch**.

## 6. Menu behaviour (GUI)

- The preset display (green LCD in the middle strip, where the tone selector sits) shows
  `name` plus `*` when any S parameter differs from the loaded preset.
- Click the display: categorised menu (category → submenu of presets). `<` `>` arrows step
  through the flat list. Mouse wheel over the display also steps.
- Selecting a preset sends every changed parameter to the host as a gesture (so automation and
  undo see it) and marks the state dirty.
- The 2 × 14 tone-selector buttons are drawn as on the hardware; clicking one loads the
  corresponding **factory tone into that line only** (keeps the other line), the
  "Panel" button does nothing (the panel is always live), Memory 1–4 are user slots
  (store with shift-click, recall with click), saved in the plugin state [A].
