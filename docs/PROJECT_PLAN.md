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
| 1.2 | PolyBLEP saw + start pulse, pulse with PW/PWM, impure sine, phase-locked | ✅ |
| 1.3 | Shared noise source into the filters | ✅ |
| 1.4 | TPT SVF HPF→LPF, linear Vf law with key tracking, Q(fc) damping, 7.6 kHz pole | ✅ |
| 1.5 | Nonlinear integrators (option B, Newton/tanh in the SVF loop) | ⬜ |
| 1.6 | IL/AL filter EG and ADSR with RC curves and stated time ranges | ✅ |
| 1.7 | VCA with dynamics (init/after level), sine path, VCF level | ✅ |
| 1.8 | Per-card calibration records + OU drift (Drift control) | ✅ |
| 1.9 | 2× oversampling, half-band decimation of the mono bus | ✅ |
| 1.10 | 4× mode for audio-rate sub-osc / ring mod settings | ⬜ |
| 1.11 | VCO option C (physical relaxation core) if scope data shows curved reset | 🔬 |

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
| 2.8 | Sustain I/II, sustain time, pedal | ✅ |
| 2.9 | Ring modulator with monophonic AD envelope | ✅ |
| 2.10 | Chorus/tremolo (BBD-style, option A) | 🟡 values are defaults; OE1/OE2 pages unread |
| 2.11 | BBD model (option B: clocked S&H, Holters–Parker) | ⬜ |
| 2.12 | Wah circuit on EXP pedal (PRA) | ⬜ |
| 2.13 | Reverb [A] Dattorro plate | ✅ |
| 2.14 | Reverb: study of the Lexicon 224 algorithms (README promise) and a 224-style hall | ⬜ |

### WP3 — Plugin (CLAP)
| ID | Issue | Status |
| --- | --- | --- |
| 3.1 | Params ext from the single Params table; value↔text | ✅ |
| 3.2 | Note ports (CLAP + MIDI dialects), poly pressure, channel pressure, bend, CC1/11/64 | ✅ |
| 3.3 | State save/load as `key=value` text (+ memories) | ✅ |
| 3.4 | Host gestures for GUI edits and preset loads | ✅ |
| 3.5 | CLAP voice-info / note-end events (polyphonic modulation) | ⬜ |
| 3.6 | MPE (per-note pitch/timbre as ribbon/brilliance) | ⬜ |

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

### WP5 — UI (spec 06)
| ID | Issue | Status |
| --- | --- | --- |
| 5.1 | Panel layout table + CS-80 look (colour caps, paddles, rockers, walnut cheeks) | ✅ |
| 5.2 | Per-control dirty redraw over a cached static layer | ✅ |
| 5.3 | Tooltip/value readout in the header | ✅ |
| 5.4 | Ribbon strip and clickable keyboard | ✅ |
| 5.5 | HiDPI scaling (CLAP `set_scale`), resizable window | ⬜ |

### WP6 — Validation (spec 07)
| ID | Issue | Status |
| --- | --- | --- |
| 6.1 | `tannhauser_dsp_test` T1–T15 | 🟡 see test source for the implemented subset |
| 6.2 | `tannhauser_gui_test` offscreen render + interaction | ✅ |
| 6.3 | `tannhauser_render` offline WAV renderer | ✅ |
| 6.4 | Reference-audio calibration loop (Acidus-style fit tools) | 🔬 |

### WP7 — Open hardware questions (from CS-80 compendium §10.1)
| ID | Question | Current default (where) | Status |
| --- | --- | --- | --- |
| P-1 | **Time-row polarity of the factory matrices.** Doc §14.5 infers "higher V = shorter time" from plucked tones (A/R rows at 10 V). This fails for String 1 (VCA A 8.2 V → 3.5 ms attack) and Brass (R 10 V → 10 ms). The opposite polarity fails worse (Harpsichord release 10 s). Evidence for "higher V = longer": SUB-board sustain diodes OR a *higher* voltage onto RF/RA when the pedal lengthens release. Resolve with a recording of factory String 1/Harpsichord 1, or the TWS/SUB scaling. | inverted (02 §9) | 🔬 |
| P-2 | Slider → time law and curve shapes | exponential, RC curves (02 §5) | 🔬 |
| P-3 | HPF/LPF cutoff law at the slider and tracking | linear Vf, 100 % KV tracking (02 §4) | 🔬 |
| P-4 | HPF:LPF modulation ratio (0.32–0.47 weights) | kHP = 0.47 on slider + mods | 🔬 |
| P-5 | Sub-osc, PWM LFO, ring-mod, chorus rates | 03 §3/4/9/11 | 🔬 |
| P-6 | Saw start pulse shape per card | 2 %, +0.2, ±30 % | 🔬 |
| P-7 | Ribbon range, scoop depth/speed, detune range | ±1 oct, 2 st/60 ms, 12 Hz | 🔬 |
| P-8 | Velocity extraction from one FSR signal | MIDI velocity | 🔬 |
| P-9 | Bus-to-button order on T51–T54 | printed order | 🔬 |
| P-10 | Row 17 ("wave symbol") identity | sine level (inferred from M pin LP1) | 🔬 |
| P-11 | Q law between the two IC data points | exponential | 🔬 |
| P-12 | Sustain level row 20 for Organ (20 %) looks wrong | as decoded | 🔬 |

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

## Progress log

- 2026-10-09 — Spec condensed from the compendiums; project bootstrapped; first complete
  engine, presets, GUI and tests (see statuses above).
