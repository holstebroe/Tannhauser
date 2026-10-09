# Tannhauser

Tannhäuser is a Yamaha CS-80 inspired synth emulation with a built-in reverb for those lush
space pads. It is a `.clap` instrument written in plain C++17 with no third-party DSP or GUI
libraries, in the same family as [Acidus](https://github.com/holstebroe/Acidus) and
[Gritbaal](https://github.com/holstebroe/Gritbaal).

## What it models

The CS-80 as documented in `docs/reference/` (service-manual derived), condensed into a
build specification in `docs/spec/`:

- **8 voices × 2 complete lines** (16 voice cards), each with its own calibration record
  and slow drift.
- **Hz-per-volt VCO** (IG00153): band-limited saw with the start pulse at each reset,
  phase-locked pulse (PW 50–90 %, per-line PWM LFO) and impure sine (IG00158), footage
  16′…2′, detune in Hz so the beating is constant across the keyboard.
- **HPF → LPF** 12 dB state-variable pair (IG00156) with key-tracked linear cutoff law,
  frequency-dependent damping that never self-oscillates, and the HPF following the LPF at
  half the octaves ("always bandpass").
- **IL/AL filter envelope** centred on the cutoff slider (IG00152) and the VCA ADSR (IG00159)
  with the chips' stated time ranges.
- **Polyphonic touch**: velocity and polyphonic aftertouch to level and brilliance per voice,
  aftertouch vibrato/speed/filter, initial pitch-bend scoop.
- **Performance section**: sub-oscillator (6 waveforms, into the audio range), keyboard
  control, ribbon (relative to first touch), polyphonic portamento/glissando from the last
  played key, Sustain I/II, monophonic ring modulator with its own AD envelope,
  BBD-style chorus/tremolo, plus an added plate reverb.
- **Presets**: a software tone selector replaces the hardware buttons. 22 factory tones
  decoded from the T51–T54 resistor matrices (`FT …`), 11 two-line factory combos (`FC …`),
  and a library of ~70 presets with two-letter categories (`PD` pads, `BR` brass, `ST`
  strings, `LD` leads, `BS` bass, `KY` keys, `OR` organ, `PL` pluck, `BL` bells, `SQ`,
  `FX`), plus user presets saved as `.tpreset` text files. All built-in presets are
  loudness-matched to −18 LUFS with a per-preset Patch Gain.
- **Panel** laid out like the CS-80: two programming rows, the performance strip with the
  tone selector and preset display, the left-hand panel, ribbon and a playable keyboard.

## Building

```bash
git submodule update --init
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Linux needs `libx11-dev`. Outputs:

| Target | What |
| --- | --- |
| `tannhauser.clap` | the plugin |
| `tannhauser_dsp_test` | DSP conformance tests (`docs/spec/07_VALIDATION.md`) |
| `tannhauser_gui_test [snapshot.ppm]` | offscreen GUI + plugin state tests |
| `tannhauser_render "<preset>" out.wav [chord\|line\|swell\|bass\|all]` | offline renderer |
| `tannhauser_loudness [--fit tools/preset_gains.json]` | preset loudness report / gain fit |

`cmake --build build --target deploy_clap` copies the plugin to the folder in `$CLAPTEST`.

## Using it

- Click the green display in the tone selector for the preset menu; `<` `>` or the mouse
  wheel step through presets. A `*` means the sound was edited.
- Tone-selector buttons load that factory tone into their line only (row I → line I, row II →
  line II). MEM buttons recall a stored line; shift-click stores the current line there.
- Sliders: drag vertically (shift = fine), double-click for the default, wheel to nudge.
  Performance paddles grow when pulled down, as on the hardware.
- Ribbon: drag from anywhere; pitch is relative to where you touched (±1 octave over the
  ribbon's length).
- MIDI: velocity, polyphonic aftertouch (channel pressure is applied to all held notes),
  pitch bend (`Bend Range`), CC1 vibrato, CC11 expression, CC64 sustain (CS-80 style: it
  lengthens the release, it does not hold notes).

## Documentation

- `docs/PROJECT_PLAN.md` — work packages, open issues and hardware questions, decision log.
- `docs/spec/` — the implementation specification (architecture, voice, global sections,
  parameters, presets, UI, validation).
- `docs/reference/` — the original CS-80 and vintage-synth research compendiums and the
  decoded preset matrices.

## License

MIT License. See `LICENSE`. The panel lettering uses glyphs rasterised from Liberation Sans
(SIL Open Font License 1.1), as in Acidus.
