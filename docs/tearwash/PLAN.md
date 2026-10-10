# Tearwash 225 — implementation and testing plan

Living document, same conventions as `docs/PROJECT_PLAN.md` (which links here as WP9).
Status: ✅ done · 🟡 partial · ⬜ open · 🔬 needs research or measurement

**Goal:** a Lexicon 224-family reverb whose 224, 224X and 224XL flavours measure within the
04 §4 targets of the original firmware, shipped as `tearwash225.clap` and as Tannhäuser's
reverb, plus the existing plate as the 225 flavour.

Order of work: oracle and measurement first, then the virtual hardware, then algorithms one at a
time against the oracle, then controls, then the plugins. A layer is not built on until the one
below it passes its tests.

## Work packages

### TW0 — Specification
| ID | Issue | Status |
| --- | --- | --- |
| 0.1 | Compact spec from the compendium + BlueBox notes (`docs/tearwash/01–04`) | ✅ |
| 0.2 | This plan; link from PROJECT_PLAN.md and CLAUDE.md | ✅ |

### TW1 — Oracle and calibration
| ID | Issue | Status |
| --- | --- | --- |
| 1.1 | `tearwash_oracle` on BlueBox (pinned, out of tree) with the 224XL V8.21 ROMs: slider moves, option bits, 4-channel float renders, index with slider read-back | ✅ |
| 1.2 | Analysis library (`src/tearwash/analysis/`): stimuli, sweep deconvolution, band RT, EDT, NED, spectra, IACC, modulation | ✅ |
| 1.3 | `tearwash_calib` + calibration set `tools/oracle/calib_xl.txt` (90 cases: 22 programs, decay/crossover/treble/depth/diffusion/predelay sweeps, modulation, stereo) | ✅ |
| 1.4 | Baseline: current Tannhäuser plate vs the oracle (`reports/baseline_plate_vs_224XL.md`) | ✅ |
| 1.5 | 224X V8.1 oracle: find V8.1's warm-restart entry and program-select call (BlueBox uses 00B4 / 8163 for V8.21), adapt the machine's hooks out of tree | ⬜ 🔬 |
| 1.6 | 224 V4.4 oracle: machine with the 224 microword layout (14-bit offsets, op bits 14–15, C§12.8 port map), 20 kHz / 500 ns, 16 K DMEM, V4.4 memory map (ROM1–5, WCS at 4000h), 8255 remote-head emulation for slider codes; reuse the ARU/FPC (same boards, B:aru_fpc§1). Validate with the V4.4 diagnostics (C§13.4) | ⬜ 🔬 |
| 1.7 | Core-level bit-exact harness: run our Core and BlueBox's DSP on the same WCS image, no 8080, compare every sample (oracle tool only; ROM data stays out of the repo) | ⬜ |
| 1.8 | Store oracle metrics (not audio) as `reports/oracle_224XL.tsv` so W9 runs without ROMs | ⬜ |

### TW2 — Virtual hardware core (02 §2–4)
| ID | Issue | Status |
| --- | --- | --- |
| 2.1 | DMEM (circular 16-bit, position counter), register file, coefficient decode | ⬜ |
| 2.2 | ARU MAC bit-exact (truncation per coefficient bit, 19-bit saturation, XFER/ZERO pipeline); test W1 | ⬜ |
| 2.3 | Blocks: delay, allpass (k quantised separately), one-pole pair, 2-tap fractional crossfade; W2 | ⬜ |
| 2.4 | FPC input (gain ranging, 4 steps) and output (normalise ≤ 3 shifts, divider); W3 | ⬜ |
| 2.5 | Float "clean" variant of the Core [A] | ⬜ |

### TW3 — System layer (02 §5)
| ID | Issue | Status |
| --- | --- | --- |
| 3.1 | Streaming polyphase resampler host ↔ core (arbitrary ratio, rate follows program on X/XL) | ⬜ |
| 3.2 | 7-pole Cauer filters fitted to the three nulls (224) and the 15 kHz edge (224X) | ⬜ |
| 3.3 | Pre-/de-emphasis: 224 shelf fitted to +2.6 dB @ 2 kHz, +8.15 dB @ 8 kHz; 224X 50/12.5 µs | ⬜ |
| 3.4 | L/R half-sample skew, DAC multiplex order, output AC coupling | ⬜ |
| 3.5 | Transformers (linear + tanh, 2× oversampled), idle noise; unit-variation seed | ⬜ 🔬 |

### TW4 — Algorithms (03 §1), one program at a time, each to the 04 §4 targets
| ID | Issue | Status |
| --- | --- | --- |
| 4.1 | Analysis tool: decoded program → network graph (allpass chains, delays, taps, filters) as a readable report, for the implementer (output under `build/`, not committed, D-T3) | ⬜ |
| 4.2 | 224XL CONCERT HALL (reference hall; 105 steps, 4 distinct outputs) | ⬜ |
| 4.3 | 224XL PLATE, ROOM, CHAMBER (one per algorithm family) | ⬜ |
| 4.4 | Remaining 18 XL programs (splits, chorus/echo, res chords, multiband delay, inverse room) | ⬜ |
| 4.5 | 224 V4.4: the seven algorithms (keys 01, 45, 84, 06, 0C, 1C, 0E), after TW1.6 | ⬜ |
| 4.6 | 224X V8.1 programs, after TW1.5 | ⬜ |

### TW5 — Control layer (03 §2–4)
| ID | Issue | Status |
| --- | --- | --- |
| 5.1 | XL slider laws (MID curves, LF difference, crossover, treble/HF pairs, depth curves, predelay law + ramp, diffusion, definition, size map) | ⬜ |
| 5.2 | Mode Enhancement walker (update rate, 1/32-sample steps, direction re-draw, window mask, CHORUS speed table); own pseudo-random source | ⬜ |
| 5.3 | Level detector, Decay Optimisation, Dynamic Decay | ⬜ |
| 5.4 | 224 V4.4 slot laws, pot → slot assignment (Q-T6), Mode Enh / Decay Opt 1–16, the two bugs + Bug Fix [A] | ⬜ |
| 5.5 | Display values (decay seconds as the original "approximation", Hz, ms) | ⬜ |

### TW6 — Tearwash 225 plugin
| ID | Issue | Status |
| --- | --- | --- |
| 6.1 | `tearwash_engine` library + CLAP plugin (stereo in/out, params 03 §5, state, latency) | ⬜ |
| 6.2 | GUI: remote-head-style panel (224 layout C§11.5 / LARC look for XL), flavour switch, program buttons, six sliders, option buttons, display, headroom meters, overflow LED | ⬜ |
| 6.3 | Presets: per-flavour factory programs + a small library (D-T3 for XL factory codes) | ⬜ |
| 6.4 | `tearwash225_gui_test` (render, hit tests, state round trip) | ⬜ |

### TW7 — Tannhäuser integration (01 §6)
| ID | Issue | Status |
| --- | --- | --- |
| 7.1 | Replace `PlateReverb` by the engine; append `rev.model`; old states load as 225 | ⬜ |
| 7.2 | Map Decay/Tone/Pre-delay/Mix onto 224 Large Concert Hall B; reach > 10 s decays | ⬜ |
| 7.3 | Re-voice library presets, re-fit Patch Gain (T16), T12, CPU (T15) | ⬜ |

### TW8 — Validation (04 §5)
| ID | Issue | Status |
| --- | --- | --- |
| 8.1 | `tearwash_engine_test` W1–W8 in CI | ⬜ |
| 8.2 | W9 calibration regression against stored oracle metrics | ⬜ |
| 8.3 | Listening A/B (04 §6) | 🔬 |

## Baseline result (TW1.4)

See `reports/baseline_plate_vs_224XL.md`. In short: with Decay, Tone and Pre-delay fitted per
program, the plate matches the 1 kHz decay time but not the rest — see the summary scores
there and the findings in the progress log below.

## Decision log

| Date | Decision |
| --- | --- |
| 2026-10-10 | Oracle = BlueBox running the user's original ROMs, fetched and built under `build/` at a pinned commit; never vendored, never linked into a product (no licence) |
| 2026-10-10 | Impulse responses are measured with exponential sweeps + deconvolution; single-sample impulses only as a cross-check (≈ 40 dB above the 16-bit truncation floor) |
| 2026-10-10 | All oracle outputs are DC-blocked before analysis (the ARU truncation offset is removed by the hardware's output transformers) |
| 2026-10-10 | Engine = virtual 224 hardware (our own implementation of the documented arithmetic) + hand-written algorithm networks + re-implemented control laws; the plugin never runs ROM code or ROM data |
| 2026-10-10 | Tannhäuser default flavour: 224 (V4.4) Large Concert Hall B (Blade Runner era); existing presets keep the 225 plate until re-voiced |

## Open decisions and questions

| ID | Question | Default until decided |
| --- | --- | --- |
| D-T3 | **May constants read from the original microcode (delay lengths, coefficient tables, curve tables, factory slider codes) be written into our C++?** They are facts about the algorithm, but they come from proprietary ROMs. Alternative: derive every constant by black-box fitting to the oracle (much slower, less exact) | Do not commit ROM-derived tables; keep the network-graph reports under `build/`; ask the owner |
| Q-T4 | Which V4.4 key (0C, 1C, 0E) is Room A, CD Plate A, Chorus A | from TW1.6 |
| Q-T5 | V8.1 program list and differences from V8.21 | from TW1.5 |
| Q-T6 | V4.4 pot → slot assignment (slot 5 = sixth pot is confirmed) | from TW1.6 |
| Q-T7 | Transformer and filter data (no plots in the service manual copy) | [D] values, unit seed |
| Q-T8 | Modulation pseudo-random source (ROM bytes as a table) — use our own sequence with matched statistics | own xorshift |

## Progress log

- 2026-10-10 — TW0 and TW1.1–1.4: spec and plan written from the compendium and the BlueBox
  notes; BlueBox built against the supplied 224XL V8.21 ROMs (all its self-tests pass on them);
  `tearwash_oracle`, the analysis library and `tearwash_calib` added; 90-case calibration set
  rendered; baseline report of the current plate committed.
