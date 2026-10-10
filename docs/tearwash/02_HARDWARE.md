# 02 — The virtual hardware

What the engine must reproduce of the 224 family's hardware, independent of any algorithm.
The Core and System layers (01 §2) implement this chapter.

## 1. Constants

| | 224 (V4.4) | 224X (V8.1) / 224XL (V8.21) |
| --- | --- | --- |
| Microinstruction cycle | 500 ns [S C§11.1] | 30.72 MHz / 9 = 293 ns [S B:dsp_microcode§1] |
| Steps per sample | 100 (program words 28–127) [S C§13.1] | loop length *L* = 100–109, set per program by its RESET step [R B:dsp_microcode§4] |
| Sample rate | 20 000 Hz | 30.72 MHz / 9 / *L* (CONCERT HALL 105 → 32 507.94 Hz; PLATE 100 → 34 133.33 Hz) |
| Delay memory | 16 384 × 16 bit (0.82 s) [S] | 224X 32 K; V8.21 programs need 64 K × 16 bit [R B:CLAUDE] |
| Program memory (WCS) | 128 × 32 bit [S] | 128 × 32 bit |
| Converters | 12-bit mantissa + 2-bit gain range in and out (FPC) [S C§11.3] | same FPC [S B:aru_fpc§1] |
| Audio bandwidth | ≈ 8 kHz: 7-pole elliptic, nulls 10.240 / 11.815 / 19.120 kHz [S C§11.2] | ≈ 15 kHz; spec 20 Hz–15 kHz ±1.5 dB [S C§1] |
| Pre-emphasis | +2.6 dB at 2 kHz, +8.15 dB at 8 kHz; mirrored de-emphasis [S C§11.2] | H(s) = (1 + s·50 µs)/(1 + s·12.5 µs), +12 dB asymptote; mirrored [S B:CLAUDE] |
| Control CPU | 8080A at 2.048 MHz [S] | 8080A at 2.048 MHz, one wait state per off-board cycle [R B:dsp_microcode] |
| Inputs / outputs | 2 in, 4 out A–D; A/C = stereo pair, B/D = rear [S C§3] | same |

## 2. Execution model

One pass of the microprogram per sample; every step does one multiply-accumulate and at most
one bus operation [S C§11.3, B:dsp_microcode§3]:

- **Bus op**: memory read (DMEM → bus), memory write (result register → DMEM), I/O (ADC in,
  DAC out to any of A–D, X registers to/from the 8080), or none.
- **DMEM address** = current position − offset (16-bit offsets on the X; 14-bit on the 224). The
  position counter advances once per sample, so **every delay is an integer number of samples**;
  a delay of *d* samples is a write at offset *o* and a read at offset *o + d* [S].
- **Register file**: 4 × 16-bit; written from the bus every step (address 3 = pass-through); the
  multiplier reads one register (a register written this step is read transparently) [R].
- **Coefficient**: sign + 6-bit magnitude *m*, gain *m*/32 (0 … 1.96875) [S C§12.3, R B:dsp_microcode§5].
- **Pipeline**: XFER in step *n* loads the result register with the sum of products through step
  *n* − 1; ZERO in step *n* restarts the sum with step *n*'s own product [R B:aru_fpc§2].

Building blocks that recur in every program and that the Core must provide with the same
arithmetic:

| Block | Form | Evidence |
| --- | --- | --- |
| Delay | integer length, 16-bit storage | [S] |
| Allpass | w = x + g·d (stored), y = k·d − g·x, g = m/32, **k = round(32·(1 − g²))/32** quantised separately, so the section is only approximately allpass | [R B:dsp_microcode§5: 135 of 138 sections] |
| One-pole low-pass | y = a·x + (1 − a)·y₁ with a = m/32 and 32 − m as two coefficients | [R B:parameters§2] |
| Fractional tap | two reads at offsets N and N − 1, weights w and 32 − w; the controller moves w (and N on wrap) | [S C§12.4, R B:parameters§5.3] |
| Gain / sum | coefficient up to 63/32; sums accumulate in the ARU | [S] |

## 3. ARU arithmetic

The multiply-accumulate must be bit-exact (it sets the noise, limit cycles and DC behaviour):

- The data bus carries the complement of the 8080's value; in 8080 terms the accumulator holds
  1/8 LSB, ZERO sets it to −1, each set bit *b* of *m* adds ±(((8x + 7) >> (5 − b)) + 1), the sum
  saturates to 19 bits, and XFER takes acc >> 3 [R B:aru_fpc§2–3, CLAUDE].
- Net effect: products are truncated (biased) per bit of *m*; the bias recirculates and leaves a
  **DC offset of a few LSB** on every output [R M:baseline §0]. The real unit's output
  transformers block it; the System layer's output coupling must too.
- **Overflow saturates** (SAT line, 19-bit clamp), it does not wrap [R]. Self-oscillation at
  extreme settings is allowed; no clamping of decay controls [S C§6].
- Acceptance: the Core reproduces every ARU self-test vector of the ROM sets (16–20 vectors per
  set, identical across 224 V3.2–224XL V8.21) [R B:aru_fpc§1]. The test reads the vectors from
  the user's ROM images when present and is skipped otherwise (no ROM data in the repo).

## 4. Converters (FPC)

- **Input** [S C§11.2–11.3]: per sample, comparators on the rectified input choose a gain of 0, 6,
  12 or 18 dB (thresholds 5.0 / 2.24 / 1.12 / 0.56 V of a 5 V full scale: a signal below 45 % of
  the range gets 6 dB more); the 12-bit conversion is shifted left 4 … 1 places into the 16-bit
  word, so the step is 16, 8, 4 or 2 LSB. Low bits are zero, not dithered.
- **Output**: the 16-bit word is shifted left until its top two bits differ (at most 3 shifts),
  the top 12 bits go to the DAC (dropped bits truncated [I]), and the shift count selects a
  1, ½, ¼, ⅛ divider. So the output noise floor rises with level.
- **Timing**: L and R are converted in separate halves of the sample (R 25 µs = ½ sample after
  L on the 224) [S, order I]; one DAC is multiplexed over A–D [S].
- **Headroom meter** = the gain-range comparators (5 LEDs, 6 dB apart) [S]; the X/XL firmware's
  level detector (Decay Optimisation, Dynamic Decay) reads the same comparator code [R B:parameters§4].

## 5. Analog chain

| Stage | Model | Evidence |
| --- | --- | --- |
| Input transformer | linear high-pass + soft saturation (option B of C§8.3), parameters [D] until measured | data unknown [S C§11.2] |
| Buffer, gain trim | +13 dBm = 5 V peak = ADC full scale; engine full scale 0 dBFS ≙ ADC full scale | [S] |
| Anti-alias | 224: 7-pole Cauer, passband edge ≈ 8 kHz, transmission zeros at 10.240, 11.815, 19.120 kHz (fit poles to those zeros and the edge; ripple [D] 0.5 dB); 224X: same order, edge ≈ 15 kHz [I] | [S C§11.2] |
| Pre-emphasis | 224: first-order shelf fitted to +2.6 dB @ 2 kHz and +8.15 dB @ 8 kHz (from 15.4 k, 33.2 k, 48.7 k, 2400 pF); 224X: 50 µs / 12.5 µs shelf | [S] |
| Aperture correction | compensates the hold's sinc droop; omit (the digital model has no hold droop) | [S] |
| Output | de-emphasis (exact inverse), reconstruction filter (same Cauer), level, output transformer (×2.5 voltage), AC coupling | [S C§11.2] |
| Noise | factory limits: < −80 dB re +12 dBm unweighted, < −92 dB A-weighted; THD+N < 0.05 % at +12 dBm 1 kHz | [S C§11.4] |

## 6. Controller timing (what the 8080 does while audio runs)

- Slider moves reach the firmware as 8-bit codes (ADC0817 on the 224; LARC on the XL) and are
  applied by rebuilding the affected coefficients; the audio program keeps running [S C§11.3,
  R B:parameters§6].
- Predelay changes ramp: the delay's coefficients step down to 0, the offset is rewritten, and
  they step back up (XL) [R B:parameters§2].
- Mode Enhancement: the 8080 rewrites the crossfade weights of the modulated taps ≈ 980 times a
  second (XL, CHORUS 80), 1/32 sample per update, direction re-drawn from a pseudo-random byte
  every 8·N updates, excursion bounded by a window mask [R B:parameters§5.3]. On the 224 the two
  controls are documented as "time between updates" and "step size", 1–16 each [S C§3].
- The X/XL rates follow the 8080's own speed (wait states included); the engine models them as
  fixed update rates in core time [D], measured from the oracle.
