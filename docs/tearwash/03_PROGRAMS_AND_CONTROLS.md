# 03 — Programs and controls

## 1. Programs per flavour

### 224 (V4.4) — nine programs on seven algorithms [S C§3, C§12.2]

| # | Program | Algorithm key | Predelay ms | Input | Notes |
| --- | --- | --- | --- | --- | --- |
| 1 | Small Concert Hall B | 01 | 24–152 | stereo | = algorithm of P3; best at 1.5–5 s |
| 2 | Vocal Plate | 45 | 0–107 | stereo | = algorithm of P5 |
| 3 | Large Concert Hall B | 01 | 24–152 | stereo | **Tannhäuser default** (01 §6) |
| 4 | Acoustic Chamber | 84 | 25–255 | mono | diffusion fixed |
| 5 | Percussion Plate A | 45 | 0–107 | stereo | high initial density |
| 6 | Small Concert Hall A | 06 | 24–152 | stereo | brighter than P1 |
| 7 | Room A | 0C / 1C [I] | 24–255 | stereo | wide |
| 8 | Constant Density Plate A | 0C / 1C [I] | 5–185 | summed mono | no Decay Opt |
| 9 | Chorus A | 0E [I] | 0–253 | stereo | 8 voices, random delay walk |

Select code → key: 01, 04 → 01; 02, 10 → 45; 08 → 84; 20 → 06; 09 → 0C; 0C → 1C; 21 → 0E
[R C§12.2]. Which of 0C / 1C / 0E is P7 / P8 / P9 is not yet fixed (PLAN Q-T4). V4.4 ships no
stored presets; settings are slider positions [S C§13.2].

### 224XL (V8.21) — 22 factory programs [R B:emu/README]

CONCERT HALL, BRIGHT HALL, DARK HALL, ROOM, SMALL ROOM, CHAMBER, RICH CHAMBER, PLATE, SMALL PLATE,
CD PLATE A, CD PLATE B, CHORUS&ECHO, RES CHORDS, M BAND DELAY, HALL/HALL, PLATE/PLATE,
PLATE/HALL, PLATE/CHORUS, RICH PLATE, DARK CHAMBER, INVERSE ROOM, RICH SPLIT. They run on 20
microcode records; loop lengths 100–109 steps. Outputs: 13 programs drive four distinct DACs, 9
write A = D and B = C.

### 224X (V8.1) — 11 records [S C§12.1]

Same hardware as the XL; the V8.1 program set is the XL set minus the V8.2 additions (record 81
INVERSE ROOM is XL-only). The exact V8.1 list and its differences from V8.21 are PLAN Q-T5.

### 225 [A]

The Tannhäuser plate: Decay, Tone, Pre-delay as in CS-80 spec 03 §12.

## 2. Slider codes

Every 224-family control is an 8-bit code *v* = 0 … 255 (the slider ADC). Most laws use the
5-bit step **s(v) = v >> 3, with 0 read as 1** [S C§12.5, R B:parameters§2]. The plugin stores
codes (normalised *v*/255) and displays the law's value; automation moves codes, so the
original's stepping is kept (smoothing is [A], off by default).

## 3. Control laws

### 224XL (V8.21) — exact, from the firmware's builders [R B:parameters§2–5]

| Control (page.slider) | Law |
| --- | --- |
| MID DECAY (1.2) | per tank coefficient: m = s(v), or half of a row of the curve table (9 rows × 5 points at s = 0, 8, 16, 24, 32, linear interpolation) |
| LF DECAY (1.1) | signed coefficient s(LF) − s(MID) added to the loop through the crossover low-pass: below the corner the loop follows LF, above it MID |
| CROSSOVER (1.3) | one-pole a = s(v)/32 (pair m, 32 − m): f_c = −f_s·ln(1 − a)/2π (164 Hz at s = 1 … ≈ full band at 31, CONCERT HALL f_s); register clamped ≥ 08; FF ramps both to 0 = crossover off |
| TREBLE DECAY (1.4), HF BANDWIDTH (3.4) | complementary pair m = s(v), 32 − m (FD–FE → 32/0) |
| DEPTH (1.5) | four 4-point curves sampled at 3v (knots at v = 0, 55, AA, FF) → four tap gains; curve set per program |
| PREDELAY (1.6) | millisecond law t(v): v (< 50), 50 + 2(v − 50) (≤ 99), 150 + 4(v − 100) (≤ 149), 350 + 8(v − 150); d = 34·t samples; or u·v with u = 4 … 544 samples per step from the program descriptor; changes ramp (02 §6) |
| DIFFUSION (3.5) | allpass g = min(cap, ⌊scale·(v >> 2)/4⌋/2)/32 per section, k = round(32(1 − g²)); CONCERT HALL: 00 → 0, 40 → 10/32, 12/32, 80 → 20/32, 24/32, C0 → 26/32 |
| DEFINITION (3.6) | x = (FF − v) >> 2 sets a second allpass group and caps the MID-dependent allpass gains |
| CHORUS (3.3) | modulation speed: update divider and step (≈ 2 samples/s at 00 … ≈ 475 at FF; factory 80 ≈ 30) |
| SIZE (13.1) | stretches the program's delay map in bands, ×1 … ×4 (halls) to ×10.9 (rich chamber); RT scales with it |
| MODE ENH / DECAY OPT / DYN DECAY | options; see §4 |

Measured decay law, CONCERT HALL, LF = MID, 1 kHz octave [R M:baseline §2]:

| Code | 10 | 30 | 50 | 70 | 90 | B0 | D0 | F0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| RT s | 0.78 | 1.28 | 1.89 | 2.79 | 3.92 | 5.63 | 8.67 | 17.3 |

### 224 (V4.4) — from the firmware, slot level [R C§12.5, C§13.3]

| Slot | Law | Likely pot (unconfirmed, Q-T6) |
| --- | --- | --- |
| 1 | loop gain via the 9 × 5 curve table, value s(v) 1–31 | MID |
| 2 | signed difference coefficient (mid − bass); at low codes also rewrites 3-word crossfade groups | BASS |
| 3 | complementary pair x, 32 − x | CROSSOVER |
| 4 | complementary pair | TREBLE DECAY |
| 5 | four gains, piecewise linear through knots at raw 0, 85, 171, 256 | DEPTH |
| 3F6D | fine predelay crossfade | PREDELAY |

Displayed decay and ranges on the remote [S C§3]: Bass and Mid 0.6–70 s, Crossover and Treble
100 Hz–10.9 kHz, Depth 0–71, Diffusion 0–63 (non-monotonic density: fastest near 32–37), Mode
Enh amount and pitch 1–16, Decay Opt 1–16. The display is an approximation; calibrate to
measured RT, not to the number [S C§8.6].

## 4. Options

| Option | Behaviour | Evidence |
| --- | --- | --- |
| Mode Enhancement | stepped random walk of the fractional taps (02 §6); toggling it resets the program (tail cut) | [S C§3, R B:parameters§5.3] |
| Decay Optimisation | input-level detector (gain-range code) lowers allpass gains of the MID group as level rises; not in P8/P9 (224) | [S C§3, R B:parameters§5.4] |
| Dynamic Decay (XL) | decay ramps between DECAY and STOP DCY values driven by the level detector; off in every factory program | [R B:parameters§5.5] |
| Rear Outs | outputs B/D instead of A/C; in P2, P5, P8, P9 swaps L/R (224) | [S C§3] |
| Bugs | Hall B bass-decay error and Chorus right-channel pop (224): faithful mode keeps them, Bug Fix [A] removes them | [S C§6] |

## 5. Plugin parameters (Tearwash 225)

Stable ids, append only (Tannhäuser rule). Codes are stored as *v*/255.

| Id | Key | Name | Range | Flavours |
| --- | --- | --- | --- | --- |
| 0 | `flavour` | Flavour | 224 / 224X / 224XL / 225 | all |
| 1 | `program` | Program | index into the flavour's list | 224, X, XL |
| 2 | `bass` | Bass (LF) Decay | code | 224, X, XL |
| 3 | `mid` | Mid Decay | code | all (225: Decay) |
| 4 | `xover` | Crossover | code | 224, X, XL |
| 5 | `treble` | Treble Decay | code | all (225: Tone) |
| 6 | `depth` | Depth | code | 224, X, XL |
| 7 | `predelay` | Predelay | code | all |
| 8 | `diffusion` | Diffusion | code | 224, X, XL |
| 9 | `modeenh` | Mode Enhancement | on / off | 224, X, XL |
| 10 | `chorus` | Mode Enh amount / CHORUS | code | 224, X, XL |
| 11 | `decayopt` | Decay Optimisation | on / off | 224, X, XL |
| 12 | `rear` | Rear Outputs | on / off | 224, X, XL |
| 13 | `size` | Size | code | XL |
| 14 | `definition` | Definition | code | XL |
| 15 | `hfbw` | HF Bandwidth | code | X, XL |
| 16 | `mix` | Mix [A] | 0–100 % | all |
| 17 | `ingain` | Input Gain [A] | −12 … +12 dB | all |
| 18 | `outgain` | Output Gain [A] | −∞ … +12 dB | all |
| 19 | `clean` | Clean Mode [A] (no converter noise, float core) | on / off | 224, X, XL |
| 20 | `bugfix` | Bug Fix [A] | on / off | 224 |

Program change loads the program's factory codes (224XL: the codes the oracle reads back after
loading the program [R], subject to D-T3; 224: a default set [D]) unless Immed [S C§3] is on
(exposed as a UI toggle, not a parameter).
