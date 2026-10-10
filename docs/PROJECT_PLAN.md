# Tannhäuser — Project implementation plan

Living document. It is meant to be enough to pick up any open issue **without reading the
source code or the reference compendiums**: the specification is in `docs/spec/`, the
evidence in `docs/reference/`. Update the status column and the log at the bottom whenever an
issue changes state.

Status: ✅ done · 🟡 partial / first version · ⬜ open · 🔬 needs measurement or reference audio

**Goal 1 (current):** a working `.clap` synth that sounds as close to the CS-80 as the
reference documentation allows, with a CS-80-like panel and a software preset menu.

## Document map

| Doc | Content |
| --- | --- |
| `spec/01_ARCHITECTURE.md` | Signal flow, rates, code structure, faithful vs added |
| `spec/02_VOICE.md` | Pitch law, VCO/waveshaper, noise, HPF→LPF, IL/AL EG, ADSR, VCA/touch, drift |
| `spec/03_GLOBAL.md` | Assigner, glide, sub-osc, PWM LFO, scoop, KBC, ribbon, sustain, ring mod, chorus, reverb |
| `spec/04_PARAMETERS.md` | Every parameter: id, key, range, default, flags, MIDI map |
| `spec/05_PRESETS.md` | Patch format, categories, factory decode, menu |
| `spec/06_UI.md` | Panel look, layout, controls, interaction |
| `spec/07_VALIDATION.md` | Automated and listening tests |
| `reference/…` | The original CS-80 and general emulation compendiums, decoded preset data |

## Build and run

```
git submodule update --init
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
build/tannhauser_dsp_test && build/tannhauser_gui_test
build/tannhauser_render "PD Blade Pad" out.wav        # offline render for listening
```
Output: `build/tannhauser.clap`. `cmake --build build --target deploy_clap` copies it to
`$CLAPTEST`.

## Work packages

### WP0 — Bootstrap (from Acidus/Gritbaal)
| ID | Issue | Status |
| --- | --- | --- |
| 0.1 | CMake project, CLAP submodule, C++17, size-optimised GUI/glue | ✅ |
| 0.2 | CI workflow (Linux/Windows/macOS build + tests on Linux) | ✅ |
| 0.3 | GUI infrastructure copied from Acidus (Graphics, Font, ModernDraw, atlas fonts, FileDialog, X11/Win32 glue) | ✅ |
| 0.4 | VST3 wrapper (clap-wrapper submodule as in Acidus) | ⬜ |
| 0.5 | macOS Cocoa GUI (Acidus has a stub too) | ⬜ |

### WP1 — Voice sound generation (spec 02)
| ID | Issue | Status |
| --- | --- | --- |
| 1.1 | Hz/V pitch law, footage, Hz-domain offsets, detune in KV path | ✅ |
| 1.2 | Band-limited saw + start pulse, pulse with PW/PWM, impure sine, phase-locked (4-point B-spline BLEP since 1.15) | ✅ |
| 1.3 | Shared noise source into the filters | ✅ |
| 1.4 | TPT SVF HPF→LPF, linear Vf law with key tracking, Q(fc) damping, 7.6 kHz pole | ✅ |
| 1.5 | Nonlinear integrators (option B): OTA SVF with tanh integrators, linearised predict/correct solve (02 §4), T18. V = 2.0 is a default; tune against a recording | ✅ |
| 1.6 | IL/AL filter EG and ADSR with RC curves and stated time ranges | ✅ |
| 1.7 | VCA with dynamics (init/after level), sine path, VCF level | ✅ |
| 1.8 | Per-card calibration records + OU drift (Drift control) | ✅ |
| 1.9 | 2× oversampling, half-band decimation of the mono bus | ✅ |
| 1.10 | 4× oversampling switch `os.4x` (param 97, not stored, no panel control): 4→2→1 half-band chain, noise density kept constant, T20. Not needed for the oscillators; ~6 dB less aliasing for a hard-driven resonant filter | ✅ |
| 1.11 | VCO option C (physical relaxation core) if scope data shows curved reset. Not done: Arturia's curved-saw plot is from an unnamed instrument, not the CS-80; needs a CS-80 scope capture | 🔬 |
| 1.13 | Long envelope mode [A] (`env.long`, stored): filter/VCA envelope times to 10 s / 25 s / 40 s; LONG switch in REVERB / EXTRA; preset `PD Long Blade Swell` | ✅ |
| 1.12 | CPU (T15). Done: filter cutoff/Q/line gain once per host sample, silent lines (Level or Mix 0) skipped, one shared division for both OTA gains (−17 % instructions). The nonlinear filter costs more, so the net is 0.093 → ~0.13 real-time factor at 2× (~0.26 at 4×). Left: SIMD across the 8 voices of a line (SoA rewrite of the voice loop) | 🟡 |
| 1.14 | VCO cycle-to-cycle jitter per card, scaled by Drift (02 §8), T19 | ✅ |
| 1.15 | 4-point B-spline BLEP for saw, start pulse and pulse (02 §2): worst alias −50…−55 dB → ≤ −80 dB at 2×, T17 | ✅ |

### WP2 — Global, performance and bus (spec 03)
| ID | Issue | Status |
| --- | --- | --- |
| 2.1 | 8-voice assigner (re-strike, round-robin, steal released-longest) | ✅ |
| 2.2 | Polyphonic portamento/glissando from last played key, PC clock law | ✅ |
| 2.3 | Sub-oscillator 6 waveforms, VCO/VCF/VCA depths, touch speed/VCO/VCF | ✅ |
| 2.4 | PWM LFO per line | ✅ |
| 2.5 | Initial pitch-bend scoop | ✅ |
| 2.6 | Keyboard control brilliance/level (KBC) | ✅ |
| 2.7 | Ribbon (relative), MIDI bend, master pitch, detune, mix, brilliance, resonance | ✅ |
| 2.16 | Ribbon Hold [A] (default on, "H" key left of the ribbon): bent notes keep their pitch on release, new notes unbent; Hold off = CS-80 return (03 §7, T22) | ✅ |
| 2.8 | Sustain I/II, sustain time, pedal | ✅ |
| 2.9 | Ring modulator with monophonic AD envelope | ✅ |
| 2.10 | Chorus/tremolo (BBD-style, option A) | 🟡 values are defaults; OE1/OE2 pages unread |
| 2.15 | CC64 follows the CS-80 (release extension). An optional "hold" mode for MIDI keyboards [A] | ⬜ |
| 2.11 | BBD model (option B: clocked S&H, Holters–Parker) | ⬜ |
| 2.12 | Wah circuit on EXP pedal (PRA) | ⬜ |
| 2.13 | Reverb [A] Dattorro plate | ✅ |
| 2.14 | Reverb: study of the Lexicon 224 algorithms (README promise) and a 224-style hall → now WP9 (Tearwash 225) | 🟡 oracle + baseline done |

### WP3 — Plugin (CLAP)
| ID | Issue | Status |
| --- | --- | --- |
| 3.1 | Params ext from the single Params table; value↔text | ✅ |
| 3.2 | Note ports (CLAP + MIDI dialects), poly pressure, channel pressure, bend, CC1/11/64 | ✅ |
| 3.3 | State save/load as `key=value` text (+ memories) | ✅ |
| 3.4 | Host gestures for GUI edits and preset loads | ✅ |
| 3.5 | CLAP voice-info / note-end events (polyphonic modulation) | ⬜ |
| 3.6 | MPE (per-note pitch/timbre as ribbon/brilliance) | ⬜ |
| 3.7 | Values shown in circuit units (Hz at C4, Q, V, %, st, dB, ms) and parsed back (spec 04, T21) | ✅ |

### WP4 — Presets (spec 05)
| ID | Issue | Status |
| --- | --- | --- |
| 4.1 | Generator `tools/gen_presets.py` → `src/presets/PresetData.cpp` | ✅ |
| 4.2 | 22 FT factory tones + 11 FC combos decoded from `cs80_presets.json` | ✅ |
| 4.3 | Library ≥ 60 presets with two-letter categories | ✅ |
| 4.4 | Preset menu (categories, arrows, wheel, modified star) | ✅ |
| 4.5 | User presets: Save As / scan folder | ✅ |
| 4.6 | Tone-selector buttons load factory tone per line; Memory 1–4 slots | ✅ |
| 4.7 | Re-voice library presets after A/B listening against recordings | 🔬 |
| 4.8 | Loudness normalisation: Patch Gain [A] fitted to −18 LUFS for every preset, `tannhauser_loudness`, test T16 | ✅ |

### WP5 — UI (spec 06)
| ID | Issue | Status |
| --- | --- | --- |
| 5.1 | Panel layout table + CS-80 look (colour caps, paddles, rockers, walnut cheeks) | ✅ |
| 5.2 | Per-control dirty redraw over a cached static layer | ✅ |
| 5.3 | Tooltip/value readout in the header | ✅ |
| 5.4 | Ribbon strip and clickable keyboard | ✅ |
| 5.5 | HiDPI scaling (CLAP `set_scale`), resizable window | ⬜ |
| 5.7 | Tooltips: full parameter names and a description line from the Arturia manual's wording; short panel names at one size (GUI test checks widths) | ✅ |
| 5.6 | CS-80 printing: names under the programming sliders, scale-end legends, RES H/L, VCF LEVEL, waveform symbols, keyboard-control/touch captions, cap colours from photos; sustain/porta time as LONG/SHORT sliders | ✅ |

### WP6 — Validation (spec 07)
| ID | Issue | Status |
| --- | --- | --- |
| 6.1 | `tannhauser_dsp_test` T1–T13, T15–T21 (T14 is in the GUI test) | 🟡 T4 checks only "no self-oscillation", not the peak-gain-vs-cutoff curve; T13 checks RMS, not f0 |
| 6.2 | `tannhauser_gui_test` offscreen render + interaction | ✅ |
| 6.3 | `tannhauser_render` offline WAV renderer | ✅ |
| 6.4 | Reference-audio calibration loop (Acidus-style fit tools) | 🔬 |

### WP7 — Open hardware questions (from CS-80 compendium §10.1)
| ID | Question | Current default (where) | Status |
| --- | --- | --- | --- |
| P-1 | **Time-row polarity of the factory matrices.** Doc §14.5 infers "higher V = shorter time" from plucked tones (A/R rows at 10 V). This fails for String 1 (VCA A 8.2 V → 3.5 ms attack) and Brass (R 10 V → 10 ms). The opposite polarity fails worse (Harpsichord release 10 s). Evidence for "higher V = longer": SUB-board sustain diodes OR a *higher* voltage onto RF/RA when the pedal lengthens release. Resolve with a recording of factory String 1/Harpsichord 1, or the TWS/SUB scaling. **Tested 2026-10-09** (`tools/analyze_factory_polarity.py`, spec 05 §3): inverted 62/74 plausible, direct 33/74, no row prefers direct; misfits = String tones (staccato, plausible for the factory vamp strings) and 10 ms Brass/E.Piano release. Kept inverted; a recording would still settle the strings. Re-run with the Arturia time ranges: 58/74 vs 28/74, same verdict. | inverted (02 §9) | 🟡 tested by plausibility, not by recording |
| P-2 | Slider → time law and curve shapes. Ranges now from the Arturia CS-80 V manual (A 2–580/885 ms, D to 8.75/7.35 s, R to 11/11.5 s); the curve between the end points is still assumed | exponential, RC curves (02 §5) | 🟡 ranges sourced, curve 🔬 |
| P-3 | HPF/LPF cutoff law at the slider and tracking. Arturia's manual gives HPF 26.8 Hz–16.2 kHz and LPF 37.1 Hz–22.3 kHz (~600:1, exponential, key unstated). Not adopted: it conflicts with the IG00156 linear-Vf data; settle with a hardware sweep at a known key | linear Vf, 100 % KV tracking (02 §4) | 🔬 |
| P-4 | HPF:LPF modulation (resistor weights 0.32–0.47 vs Cherry's "half the octaves") | HPF slider ×0.47, modulation = half the LPF octaves (02 §4) | 🔬 |
| P-5 | Sub-osc, PWM LFO, ring-mod, chorus rates. Sub-osc 0.5–100 Hz, PWM 0.1–127 Hz, ring mod 0.25–205 Hz and its AD 3–530 ms / 7 ms–4.5 s now from the Arturia manual; chorus still default | 03 §3/4/9/11 | 🟡 chorus 🔬 |
| P-6 | Saw start pulse shape per card | 2 %, +0.2, ±30 % | 🔬 |
| P-7 | Ribbon range, scoop depth/speed, detune range | ±1 oct, 2 st/60 ms, 12 Hz | 🔬 |
| P-8 | Velocity extraction from one FSR signal | MIDI velocity | 🔬 |
| P-9 | Bus-to-button order on T51–T54 | printed order (assignment search on envelope times gains < 1 tone per channel: no better order found) | 🔬 |
| P-10 | Row 17 ("wave symbol") identity | sine level (inferred from M pin LP1) | 🔬 |
| P-11 | Q law between the two IC data points | exponential | 🔬 |
| P-12 | Sustain level row 20 for Organ (20 %) looks wrong | as decoded | 🔬 |

### WP9 — Tearwash 225 (Lexicon 224-family reverb, own plugin + Tannhäuser reverb)
Spec in `docs/tearwash/`, plan and issue tables in **`docs/tearwash/PLAN.md`** (TW0–TW8).
| ID | Issue | Status |
| --- | --- | --- |
| 9.1 | TW0 spec + plan | ✅ |
| 9.2 | TW1 ROM oracle (BlueBox, 224XL V8.21), calibration tool, baseline of the current plate | 🟡 224X / 224 oracles open |
| 9.3 | TW2–TW5 engine (virtual 224 core, system layer, algorithms, controls) | 🟡 224XL: 5 of 22 programs bit-exact |
| 9.4 | TW6 `tearwash225.clap` with own UI; TW7 Tannhäuser integration (224 default) | 🟡 plugin built; TW7 open |

### WP8 — Later / nice to have
| ID | Issue | Status |
| --- | --- | --- |
| 8.1 | GX-1 extras (Cherry-style) — out of scope for goal 1 | ⬜ |
| 8.2 | Unison mod (KSR) as an added mode | ⬜ |
| 8.3 | Preset morph / random patch generator | ⬜ |

## Decision log

| Date | Decision |
| --- | --- |
| 2026-10-09 | Bootstrap from Acidus (more mature CLAP + GUI code than Gritbaal); new DSP written for the CS-80 |
| 2026-10-09 | Parameters stored as slider positions 0..1 (not volts); presets convert volts with the row inversions |
| 2026-10-09 | One 2× oversampled island for all voices, decimated once on the mono bus |
| 2026-10-09 | Software presets store global sections too (hardware paddles are not stored) |
| 2026-10-09 | Factory time polarity follows compendium §14.6 (inverted) until P-1 is resolved |
| 2026-10-09 | P-1 plausibility test confirms the inverted polarity (62/74 vs 33/74); kept |
| 2026-10-09 | Time and rate ranges taken from the Arturia CS-80 V manual (§5.2, documented against the hardware): envelopes, sub-osc, PWM LFO, ring mod; global Resonance bipolar. Library presets and defaults remapped to keep their voiced times; factory tones follow the new laws. Arturia's filter cutoff ranges not adopted (P-3) |
| 2026-10-10 | Ribbon Hold defaults to on (user's call): an [A] behaviour as default, the CS-80's return-to-pitch is the off position |
| 2026-10-09 | WP1 analog-modelling pass (prompted by Arturia's TAE claims): nonlinear OTA filter, cycle jitter, 4-point BLEP, optional 4×. Presets are not compensated for model changes, only loudness-refitted (user's call) |
| 2026-10-09 | Long envelope mode added as a stored per-patch switch [A] (global setting in Arturia's emulation) |
| 2026-10-09 | Loudness: presets are trimmed by an added, stored Patch Gain (not by editing decoded line levels) to −18 LUFS max-momentary |
| 2026-10-10 | Lexicon work becomes its own product, Tearwash 225 (WP9); its decisions are logged in `docs/tearwash/PLAN.md` |

## Progress log

- 2026-10-09 — Spec condensed from the compendiums; project bootstrapped from Acidus; first
  complete engine (all of WP1/WP2 except the items left open), CLAP wrapper, 102 presets
  (1 init + 22 FT + 11 FC + 68 library incl. 2 IN templates), CS-80 panel with tone selector,
  preset menu, ribbon and keyboard. `tannhauser_dsp_test` 147 checks and
  `tannhauser_gui_test` 272 checks pass; the `.clap` was smoke-tested in a dlopen host under
  Xvfb (GUI create/show/process/destroy).
- 2026-10-09 — Calibration decisions while voicing: Level slider is linear (B10K into the VCA
  LI input); the HPF slider goes through the 0.47 divider and modulators move the HPF by half
  the LPF's octaves (the first draft's volt-weighted HPF made plucks ~10 dB too thin).
- 2026-10-09 — P-1 tested (`tools/analyze_factory_polarity.py`): inverted polarity kept. Added
  Patch Gain (param 95) and loudness normalisation of all 102 presets to −18 LUFS (±0.15 LU),
  `tannhauser_loudness`, test T16; dsp test 249 checks.
- 2026-10-09 — Arturia CS-80 V manual ranges: envelope times (02 §5/§6), sub-osc/PWM/ring-mod
  rates and ring-mod AD (03), bipolar global Resonance; rates are shown in Hz. Long envelope
  mode `env.long` (param 96) [A] with a LONG rocker and `PD Long Blade Swell`. Library presets
  remapped by law (gains moved < 0.5 dB); factory tones re-fitted (+1…3 dB on the short
  plucks/organs, now 2 ms minimum times). P-1 re-run: inverted still preferred. dsp test 257
  checks, gui test 274.
- 2026-10-09 — WP1: OTA SVF with saturating integrators (1.5), VCO cycle jitter (1.14),
  4-point B-spline BLEP (1.15; aliasing ≤ −80 dB at 2×), 4× switch `os.4x` (1.10, param 97),
  CPU work (1.12 partial: −17 % instructions, real-time factor ~0.13). Tests T17–T20; presets
  loudness-refitted (median −1.3 dB). 1.11 left for scope data. dsp test 267 checks.
- 2026-10-10 — Value display in circuit units like the Arturia manual (3.7, T21); panel
  printing and cap colours from photos of the CS-80 (5.6): duplicate RES/LEVEL names fixed.
- 2026-10-10 — Ribbon Hold (2.16, params 98/99, T22); tooltips with full names and
  descriptions, shorter panel names at one size (5.7).
- 2026-10-10 — WP9 started: Tearwash 225 spec and plan (`docs/tearwash/`), BlueBox-based 224XL
  oracle (`tools/oracle/`), `tearwash_calib`, baseline report of the current plate against 22
  original programs (`docs/tearwash/reports/baseline_plate_vs_224XL.md`).
- 2026-10-10 — WP9: native 224XL networks for CONCERT HALL, ROOM, PLATE, SMALL PLATE and CHAMBER,
  bit-exact against the oracle; `tearwash225.clap` with its own panel (`docs/tearwash/PLAN.md`).
- Next suggested steps: P-1 confirmation with a reference recording; 1.5 nonlinear SVF;
  1.12 CPU; 4.7 re-voicing against recordings; 0.4 VST3; 0.5 macOS GUI.
