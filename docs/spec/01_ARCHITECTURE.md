# 01 — Architecture and signal flow

Condensed from `docs/reference/Yamaha CS-80 — Hardware-Accurate Emulation Compendium.md`
(the "CS-80 compendium", cited as §n) and the General Implementation Compendium (cited as G§n).
Everything an implementer needs is in `docs/spec/`; go to the reference documents only to
re-check evidence. Confidence tags: **[S]** sourced, **[I]** inferred, **[D]** default chosen by
this project (tune later), **[A]** added feature (not on the hardware).

## 1. The instrument in one paragraph

8 voices. Each voice is two complete, independent synth *lines* (I and II), each a separate
"voice card" (16 cards). A key always plays both lines of one voice. Each line:
VCO (linear Hz/V saw core) → waveshaper (saw, pulse, sine, all phase-locked) → 12 dB HPF →
12 dB LPF → VCA, plus a sine that bypasses the filters and white noise into the filters.
The 16 lines are summed to **mono**, then pass a monophonic ring modulator, expression,
and a BBD chorus/tremolo that is the only source of stereo. [S §1, §5, §14.2]

## 2. Signal flow

```
                    per line (x16) ───────────────────────────────────────────────┐
key → KAS key voltage (Hz/V) → glide/gliss → + detune(II) + drift(Hz) → VCO saw ──┤
                                     ↑ sub-osc VCO, touch VCO, scoop, ribbon       │
 saw ─┬─ [Saw switch] ─────┐                                                       │
      ├─ pulse(PW,PWM) ────┤ [Square switch]                                       │
      │  noise (shared) ───┤ level                                                 │
      │                    ▼                                                       │
      │        HPF (IG00156 SVF) ──► LPF (IG00156 SVF) ──► 7.6 kHz pole ──► ×VCF level
      │          ↑ Vf_H = HPF slider + 0.5 × mods      ↑ Vf_L = LPF slider + mods   │
      │          mods = FEG(IL/AL) + touch brill + global brill + kbd brill + sub VCF
      └─ sine (waveshaper) ─────────────────────────────────────────────► ×Sine level
                                                      sum ─► × VEG(ADSR) × dynamics
                                                            × level × kbd level × sub VCA
 Σ 8 voices × (line I·(1−mix) + line II·mix) ─► mono bus (2× oversampled)
   ─► ring modulator (own oscillator + AD env, monophonic) ─► decimate to host rate
   ─► expression ─► chorus / tremolo (stereo) ─► [A] reverb ─► volume ─► L, R
```

## 3. Rates and numerics

| Block | Rate | Notes |
| --- | --- | --- |
| Voice audio (VCO, waveshaper, SVFs, VCA), mono bus, ring mod | 2× host rate | [D] §9: 2× default; 4× is open issue for audio-rate sub-osc |
| Decimation | 2:1 half-band FIR on the mono bus only | All voices are summed first, so only one decimator |
| Filter envelope | every oversampled sample | IL/AL attacks can be 1 ms [S §9] |
| VCA envelope, LFOs, glide, touch smoothing | every oversampled sample (cheap) | [D] simpler than a control-rate split; optimise later |
| Chorus/tremolo, reverb | host rate | |

All coefficients are derived from physical times/frequencies so 44.1/48/96 kHz sound the same
(G§10.3). Denormals are flushed (FTZ/DAZ) in `process`.

## 4. Code structure (mirrors Acidus/Gritbaal)

```
src/core/      DSP: Params (single source of truth for parameters), Oscillator, CsFilter,
               Envelope, Lfo, Voice (line + voice card), SynthEngine (assigner, globals),
               RingMod, Chorus, Reverb, Halfband, Drift
src/presets/   Patch model, factory decode + preset library (generated PresetData.cpp)
src/clap/      CLAP plugin wrapper (params, state, note ports, audio ports, GUI ext)
src/gui/       Software-rendered panel (Graphics, Font, modern/ shading + atlas fonts),
               CS-80 layout, preset menu, X11/Win32 window glue
tools/         Generators (fonts, presets) and analysis helpers
docs/spec/     This specification
docs/reference Original research compendiums, decoded preset data
```

Plugin targets: `tannhauser.clap` (instrument). Test executables: `tannhauser_dsp_test`,
`tannhauser_gui_test`, `tannhauser_render` (offline WAV renderer for listening/validation).

## 5. Faithful vs added (G§14)

| Faithful (default) | Added, labelled [A] |
| --- | --- |
| Mono sum; stereo only via chorus/tremolo | Reverb (Lexicon-224 inspired), off-able |
| Polyphonic aftertouch per voice | Channel pressure applied to all held voices |
| Ribbon relative to first touch | MIDI pitch bend with Bend Range |
| Per-card drift and calibration error | Drift amount control (0 = perfect calibration) |
| 22 factory tones as fixed sets | Unlimited software presets with a category menu |
| Paddles not stored in tones | Software presets store global/performance sections too |
| Envelope times to ~0.6–11.5 s | Long envelope mode: attack/decay/release to 10/25/40 s |
