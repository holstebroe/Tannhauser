# 07 — Validation

Levels follow CS-80 compendium §10.2 and G§15.2: cheapest and most diagnostic first; a
failing low level invalidates everything above it.

## 1. Automated (`tannhauser_dsp_test`, run after every DSP change)

| ID | Test | Pass target |
| --- | --- | --- |
| T1 | Pitch: line at 8′, Drift 0, notes C1…C6 | measured f0 within 1 cent of `261.626·2^((n−60)/12)·feet` |
| T2 | Linear law: add a fixed offset, measure error in Hz | constant in Hz across keys (≤ 2 %) |
| T3 | Footage ratios | 0.5, 1, 1.5, 2, 3, 4 within 1 cent |
| T4 | LPF small-signal response at Res max for fc 100 Hz, 1 kHz, 5 kHz | peak gain falls with fc; < +6 dB above 5 kHz; never self-oscillates (impulse decays) |
| T5 | HPF→LPF tracking: an FEG sweep moves the HPF by half the LPF's octaves | ratio 0.5 ± 0.01 |
| T6 | Filter EG shapes | IL only: starts −5·il V, ends −5·il V; AL only: peak +5·al V, rests 0 V |
| T7 | Envelope time law | A=0 → ≈1 ms, A=1 → ≈1 s; D/R 10 ms…10 s (±10 %) |
| T8 | Per-voice touch | pressure on one voice of a chord changes only that voice |
| T9 | Ring-mod envelope | retriggers only when all keys were up |
| T10 | Sustain II | new note silences fading notes within 10 ms |
| T11 | Glide | new voice starts at the last played key's pitch |
| T12 | Robustness | all presets: 8-note chord, finite output, peak < 1.0 |
| T16 | Preset loudness | every preset within ±1 LU of −18 LUFS (spec 05 §4) |
| T13 | Sample-rate invariance | 44.1/48/96 kHz: f0 and RMS within 0.2 dB / 1 cent |
| T14 | State round trip | save → load reproduces every parameter |
| T15 | CPU | 16 lines at 48 kHz: real-time factor < 0.25 on CI hardware |

## 2. GUI (`tannhauser_gui_test`)

Renders the panel offscreen, checks the frame is non-empty, every control hit-tests to
itself, slider drags change values and emit gestures, menu opens and selects, and writes
`gui_snapshot.ppm` for visual review.

## 3. Listening / reference (manual, see plan WP9)

- `tannhauser_render <preset> <out.wav>` renders a fixed phrase (chord, line, swell with
  aftertouch ramp) for A/B against reference recordings (§3 of the CS-80 compendium:
  Blade Runner, Chariots of Fire, Spiral; Cherry GX-80 and Arturia CS-80V as differential
  references, never as constants — G§12.6).
- Checks first: Brass 1, String 1, Organ 1 factory tones (time polarity P-1, resonance
  direction, Clavichord with 0 % cutoff opened by touch).
