# Tearwash 225 — specification

Tearwash 225 is a Lexicon 224-family digital reverb: a stand-alone `.clap` plugin with its own UI,
and the reverb engine inside Tannhäuser. It offers four **flavours**:

| Flavour | Hardware it models | Core rate | Delay memory | Programs |
| --- | --- | --- | --- | --- |
| **224** | Original 224, firmware V4.4 (1979–82) | 20 kHz | 16 K words | 9 programs on 7 algorithms |
| **224X** | 224X, firmware V8.1 (1983) | 30.72 MHz / 9 / loop ≈ 31.3–34.1 kHz | 32 K (64 K for V8.2 programs) | V8.1 set |
| **224XL** | 224XL (LARC), firmware V8.21 (1984–86) | as 224X | 64 K | 22 factory programs |
| **225** | Tannhäuser's existing Dattorro plate [A] (beyond the hardware) | host rate | — | 1 |

The name follows the hardware lineage (224 → "225"); *Tearwash* is the project name.
Tannhäuser uses the **224** flavour by default: the original 224 is the unit of the Vangelis /
*Blade Runner* era (1982; the 224X shipped in 1983). See 01 §6.

## Documents

| Doc | Content |
| --- | --- |
| `01_ARCHITECTURE.md` | Products, engine layers, code layout, faithful vs added, IP rules |
| `02_HARDWARE.md` | The virtual hardware: rates, DMEM, microprogram semantics, ARU arithmetic, FPC converters, analog chain, modulation timing |
| `03_PROGRAMS_AND_CONTROLS.md` | Programs per flavour, control laws (decay, crossover, treble, depth, predelay, diffusion, mode enhancement, decay optimisation), plugin parameters |
| `04_VALIDATION.md` | The ROM oracle, stimuli, metrics, scores, acceptance targets, test list |
| `PLAN.md` | Work packages, status, decisions, open questions, progress log |
| `reports/` | Calibration reports produced by `tearwash_calib` |

Evidence sources: `docs/reference/Lexicon 224 Digital Reverberator — Hardware-Accurate Emulation
Compendium.md` (cited **C§n**) and the BlueBox reverse-engineering notes (github.com/jimbattin/bluebox
`docs/`, cited **B:file§n**), plus our own oracle measurements (`reports/`, cited **M:report§n**).
Confidence tags as in the CS-80 spec: **[S]** sourced, **[I]** inferred, **[D]** default chosen here,
**[A]** added feature; plus **[R]** measured on, or derived from, the original firmware running in
the oracle.
