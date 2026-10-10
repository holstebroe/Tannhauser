# 01 — Architecture

## 1. Products

| Output | What it is | Code |
| --- | --- | --- |
| `tearwash225.clap` | Stand-alone stereo reverb plugin, own UI, flavour switch 224 / 224X / 224XL / 225 | `src/tearwash/engine/`, `src/tearwash/plugin/` (CLAP wrapper, parameters, panel) |
| Tannhäuser reverb | The same engine as a library inside `tannhauser.clap`, default flavour 224 | `tannhauser_core` links `tearwash_engine` |
| `tearwash_calib` | Offline comparison against the ROM oracle (04) | `src/tests/tearwash_calib_main.cpp` |
| `tearwash_oracle` | Out-of-tree driver for the BlueBox emulator (04 §1) | `tools/oracle/` |

Both plugins share one engine library (`tearwash_engine`, static, no GUI, no CLAP), so a fix in
the engine reaches both and the calibration tool tests exactly what ships.

## 2. Engine layers

```
 host L,R ─► [System in] ─► resample to core rate ─► [Virtual 224 core] ─► resample to host ─► [System out] ─► wet A..D
             transformer, AA filter,                  program (algorithm) on                   12-bit DAC, divider,
             pre-emphasis, gain-ranged                16-bit DMEM + ARU MAC,                    de-emphasis, recon.
             12-bit ADC (FPC)                         one pass per sample                       filter, transformer
                                  ▲
                         [Control layer] (8080 firmware behaviour, re-implemented):
                         slider laws, coefficient builders, predelay ramps, mode-enhancement
                         tap walker, decay optimisation, dynamic decay
 output select A/C or B/D (Rear Outs), mix [A], output gain [A]
```

| Layer | Faithful content | Rate |
| --- | --- | --- |
| System (I/O) | Input transformer, 7-pole elliptic anti-alias (224: 8 kHz; 224X: ≈15 kHz), pre-emphasis, track-and-hold skew (R sampled half a sample after L), gain-ranged 12-bit ADC → 16-bit, FPC fixed-to-float output, DAC divider, de-emphasis, reconstruction filter, output transformer, idle noise | host (filters, transformers), core (converters) |
| Core | Algorithm network on a virtual 224: integer delays on one circular 16-bit memory, 6-bit coefficients (m/32), multiply-accumulate with the hardware's rounding and 19-bit saturation, 2-tap fractional crossfades | core rate |
| Control | Parameter → coefficient laws, ramps, modulation walkers, level detector | event / 1 kHz control tick (the 8080's pace) |
| Added [A] | Mix, input/output gain, flavour switch, 225 plate, host-rate "clean" mode, parameter smoothing for automation, presets | host |

The 225 flavour bypasses the System and Core layers: it is the existing host-rate plate
(`tannhauser::PlateReverb`, spec 03 §12 of the CS-80 spec), kept bit-compatible.

## 3. Rates and resampling

- Core rate 20 000 Hz (224) [S C§11.1]; 224X/XL: 30.72 MHz / 9 / *L* with *L* the program's loop
  length (100–109 steps → 34 133–31 315 Hz) [R B:dsp_microcode§4]. The rate therefore changes with
  the program on the X/XL, and the engine follows it.
- Host ↔ core: streaming polyphase Kaiser-windowed sinc resampler with arbitrary ratio, flat to
  0.45·core rate, ≥ 80 dB stop band; latency reported to the host. It also acts as the converter
  anti-alias / anti-image filter; the analog elliptic responses (02 §6) are applied on top at the
  host rate.
- Control layer timing is in core samples (or 8080 time for the X/XL modulation walker, 02 §8),
  never in host samples, so behaviour is sample-rate independent (CS-80 rule 2).

## 4. Code layout (planned; see PLAN.md for status)

```
src/tearwash/
  analysis/   Wav.hpp, Stimuli.hpp, Metrics.hpp     offline tools only
  engine/     Core.hpp/.cpp        virtual 224 core: DMEM, ARU arithmetic, FPC
              System.hpp/.cpp      transformer, elliptic filters, emphasis, converters
              Resampler.hpp        streaming polyphase resampler
              Programs*.cpp        algorithm networks per flavour
              Control.hpp/.cpp     slider laws, ramps, modulation, decay optimisation
              Tearwash.hpp/.cpp    public engine API (flavour, program, params, process)
  plugin/     TwParams.hpp/.cpp     parameter table (03 §5), value text
              TearwashClap.hpp/.cpp CLAP wrapper: engine swap, dry/wet alignment, state
              TwGui.hpp/.cpp        panel (uses src/gui Graphics, ModernDraw and fonts)
```

Plugin threading: a program or flavour change builds a new engine on the main thread and passes
it to the audio thread through an atomic slot; the replaced engine returns through a second
slot and is freed on the main thread. Register changes of the running program are applied on
the audio thread (`Engine::setControls` does not allocate). The plugin reports one latency per
sample rate, that of the longest loop (109 steps); each program's wet path and the dry path are
delayed up to it, so a program change never changes the latency.

Rules (from the Tannhäuser CLAUDE.md, unchanged): C++17, no third-party DSP or GUI libraries,
nothing allocates, locks or throws on the audio thread, coefficients from times and frequencies.

## 5. Faithful vs added

Faithful by default: integer control codes (sliders are 8-bit, most laws use code >> 3), stepped
modulation, 16-bit arithmetic with the hardware's truncation and saturation, converter noise and
gain-ranging, original bugs. Added, defaulting off where it changes the sound: Mix and dry path
(the 224 has no dry path [S C§3]), input/output gain, parameter smoothing for host automation,
"clean" converters (no gain-ranged quantisation at the ADC and DAC; the core keeps its integer
arithmetic), the 225 flavour.

## 6. Tannhäuser integration

- Tannhäuser keeps parameter ids 91–94 (`rev.mix`, `rev.decay`, `rev.tone`, `rev.predelay`) and
  appends `rev.model` (0 = 225 plate, 1 = 224 Large Concert Hall, …). States and presets without
  `rev.model` load as 225 so existing sounds do not change; library presets are re-voiced to 224
  and re-loudness-matched (T16) in WP TW7.
- In 224 mode the four Tannhäuser knobs map onto 224 controls: Decay → MID DECAY (with BASS tied,
  ratio [D] 1.0), Tone → TREBLE DECAY, Pre-delay → PREDELAY, Mix → wet level. Vangelis-length
  decays (> 10 s) must be reachable [S C§2, unverified anecdote].
- The *Blade Runner*-era choice is the original 224 with V4.x firmware (the 224X arrived in 1983,
  the NVS card with V4.4 around 1981–82) [I C§1].

## 7. ROM and BlueBox rules (IP)

- ROM images are not in the repository. The user supplies them as a zip in the git-ignored
  `training/lex/` for a comparison session (`tools/oracle/README.md`). They are used **only as a reference**:
  by the out-of-tree oracle and by analysis tools. They are never copied into `src/`, never
  embedded in a plugin, never loaded at runtime, and no ROM byte tables are committed.
- BlueBox has no licence: it is fetched and built under `build/` (git-ignored) by
  `tools/oracle/setup_bluebox.sh`, never vendored, never linked into a product.
- The engine's algorithms are our own C++ implementations. Their structure and constants are
  established from public documentation, from measurement against the oracle, and from analysis
  of the original programs; each derived value is tagged **[R]** in 03. Whether constants read
  out of the original microcode may be written into our code is an open decision (PLAN.md D-T3).
