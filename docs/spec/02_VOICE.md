# 02 — Voice line (one voice card)

One *line* = one CS-80 "M" voice card. Each of the 8 voices owns two lines (I, II) with
independent parameter sets (doc 04, prefixes `l1.` and `l2.`). Slider positions are 0..1
("up" = more). The hardware sliders are B10K (linear) feeding 0–10 V; where this document
says *V* it means `10 × position` unless a row is inverted (see §8).

## 1. Pitch [S §13 key-voltage law, §14.3]

- Key voltage is a **Hz-per-volt** law: `f = 523.2 Hz/V × KV` at 8′. Middle C (MIDI 60,
  Yamaha "C3") = 261.63 Hz at 8′; the KAS note ladder is exactly equal-tempered.
  The engine therefore computes the *ideal* frequency `f0 = 261.626 × 2^((n−60)/12) × tune`
  and treats it as KV.
- **Feet** (6 positions) multiply frequency: 16′ ×0.5, 8′ ×1, 5⅓′ ×1.5, 4′ ×2, 2⅔′ ×3, 2′ ×4.
- **Linear-law consequences.** Offsets are added in **Hz**, not cents:
  `f_line = (f_key × feet) × (1 + scaleErr) + offsetHz + drift(t) [+ detuneHz for line II]`.
  A constant Hz error is a big pitch error in the bass, small in the treble (§7.2). [S]
- **Detune** (global, acts on line II only): `detuneHz = 12 Hz × d²`, d = slider 0..1,
  applied in the key-voltage path (so it is multiplied by the footage ratio). Constant beat
  rate across the keyboard is the intended CS-80 character. [I topology, D range]
- **Exponential pitch modulators** (applied as a ratio after the linear sum):
  ribbon / pitch bend, sub-osc VCO depth (vibrato), touch VCO (aftertouch vibrato depth),
  initial pitch bend (scoop). [D: VIB pin law unknown]
- **Master pitch** ±1 semitone [S approx.]; global tuning only.

## 2. VCO and waveshaper [S §6.1, §13 IG00153/IG00158; option A of §9.2]

- Phase accumulator `φ ∈ [0,1)`, increment `f/fs_os`. Saw = `2φ − 1`.
- **Band limiting**: saw and pulse are rendered naive into a 2-sample delay line and every
  jump (saw reset, start-pulse edges, pulse edges) gets a **4-point BLEP**: the step smoothed
  by a cubic B-spline (support ±2 samples) minus the ideal step, added to the four samples
  around the jump at its exact fractional time. Worst alias image at 2× ≤ −80 dB re the
  fundamental for a G7 saw/pulse with the filter open (test T17; the 2-point PolyBLEP used
  before 2026-10-09 gave −50…−55 dB). Cost: the B-spline's passband droop, −0.6 dB at 10 kHz
  and −2.5 dB at 20 kHz (at 96 kHz), well under the 7.6 kHz pole that follows the LPF. The sine
  uses the phase two samples back so it stays aligned. [D method]
- **Saw start pulse**: a short pulse added at each reset, width `w = 2 %` of the period
  (min 2.5 samples), height `+0.2`, both randomised ±30 % per card [D §14.6], both edges
  band-limited as above.
- **Pulse** = comparator of the saw against the PW threshold: goes high at the reset, low when
  the phase passes PW (a PW change cannot retrigger it within the cycle).
  Width `PW = 0.5 + 0.4 × pw` (50 %…90 %) [S], plus PWM: `+ 0.4 × pwmDepth × lfo` (sine LFO
  per line, see doc 03 §4), clamped to [0.5, 0.95] (the waveshaper cannot go below square).
- **Sine** from the triangle derived from the same phase, shaped
  `sin(π/2 · tri)` plus a small impurity `+0.03·tri³` (2nd/3rd-harmonic residue). [S impure
  sine; D amount]
- Levels follow IG00158: saw 3.5 Vpp → ±1.0; pulse ±1.0; sine 3 Vpp → ±0.86.
- **Saw** and **Square** are on/off rockers. Both off = only noise and sine remain.
- Phase is free-running: never reset on note-on (analog VCO). Initial phase random per card.

## 3. Noise [S SUB board]

One white-noise source shared by all 16 lines (identical sample in every voice), level per
line: `noise × 0.7` into the HPF input, with a ~1 ms smoothing on the level control.

## 4. Filters: HPF → LPF (IG00156 ×2) [S topology §6.2, §13; option A of §9.2]

Each filter is a 2-pole state-variable filter with **saturating OTA integrators** (option B of
§9.2; plan 1.5). HPF output feeds LPF. A fixed one-pole low-pass at **7.6 kHz** follows the
LPF (the extra real pole ωi of §6.2).

```
u1 = x − k·bp − lp          bp' = ω·T(u1)          lp' = ω·T(bp)          hp = u1
T(v) = V·tanh(v/V),  V = 2.0 (signal units; the oscillators are ±1) [D]
```

Trapezoidal (TPT) discretisation. Each sample the OTA gains `tanh(v/V)/(v/V)` are linearised
at an estimate of v and the linear TPT system is solved exactly, twice: predicted with the
gains carried from the previous sample, corrected with the gains at the predicted voltages
(a cheap stand-in for a Newton solve; stable since the gains are ≤ 1). Small signals give the
linear SVF exactly; at high levels the resonant peak compresses and odd harmonics appear.
LPF, Q 5, sine at fc (V = 2.0): amplitude 0.001 → +14.0 dB, 0.3 → +11.2 dB (H3 −45 dBc),
1 → +4.0 dB (H3 −39 dBc), 2 → −1.1 dB (test T18 checks the same law at V = 1.2). V is a default: 1.2 made hard-driven resonant
patches alias at −51 dB at 2×; 2.0 gives −60 dB and keeps normal levels near-linear. This
replaces the input-only `1.2·tanh(x/1.2)` of the first version.

**Cutoff law (linear in Vf, keyboard-tracked)** [S IC data p.42: KV 0.25 V & Vf 5 V → 1 kHz]:

```
fc = 1000 Hz × (Vf / 5 V) × (f_key8 / 130.81 Hz) ,   f_key8 = key frequency at 8′ (no footage)
```

`fc` is floored at 20 Hz (the ω0 = 2π·23 Hz control floor) and capped at 0.45 × fs_os.
So at Vf = 10 V the cutoff is ~15.3 × the 8′ fundamental; tracking is 100 % and done by the
key voltage, not by footage.

*Not adopted:* the Arturia CS-80 V manual (§5.2.1.3) lists HPF 26.8 Hz–16.2 kHz and LPF
37.1 Hz–22.3 kHz. Both spans are ~600:1, i.e. an exponential slider map in that emulation, with
no key stated. That conflicts with the IG00156 data behind the linear law above, so the law
stays; plan P-3 records the conflict.

**Control-voltage sums** (volts, summed before the law — G§2 "sum in the circuit domain"):

```
mods  = FEG(t) + Vbrill_global + Vtouch_brill + Vkbd_brill + Vsub_vcf
Vf_L  = 10·lpf + mods                                   clamp [0, 20] V
fc_L  = law(Vf_L)
fc_H  = max(20 Hz, law(0.47 · 10·hpf)) · sqrt((Vf_L + 1) / (10·lpf + 1))
```

The HPF slider reaches its control pin through the 0.47 divider [S M board p.41: buffer →
HPF Vfc 100 kΩ, LPF Vfc 47 kΩ]. The modulators move the HPF by **half the octaves** they move
the LPF [S Cherry Audio, measured on two units; §7.1] — implemented in the octave domain
(`sqrt` of the LPF's control ratio, with a 1 V offset so a closed LPF slider does not explode
the ratio) [D]. This keeps an HPF set to 0 out of the way while still giving the
"always-bandpass" movement. (Plan P-4: the R51/R52 0.32 weights are not used.)

with: `FEG` from §5 (−5…+5 V); global Brilliance ±4 V; touch brilliance
`4 V·initBrill·velocity + 5 V·afterBrill·pressure`; keyboard brilliance ±3 V (doc 03 §6);
sub-osc VCF `±5 V·depth`. Both Vf are clamped to [0, 20] V.

**Resonance (damping only, never self-oscillates)** [S IC data: Q 5 at VQ 0 V, 0.5 at 10 V]:

```
VQ   = 10 V · (1 − res) + 10 V · (−globalResonance)        clamp [0, 10]
QA   = 5 · 0.1^(VQ/10)                     (exponential between the two data points) [D]
m    = 14.4 / QA ;  fq = 1100 Hz · m       (§6.2: QA·fq = 14.4 × 1.1 kHz is invariant)
Qeff = QA/(1 + fc/fq) + 0.5·(fc/fq)/(1 + fc/fq)           (Q falls to 0.5 above fq)
SVF damping k = 1/Qeff
```

Passband gain: LPF ×1.0 (the IC's A = 1.7 is folded into the line level normalisation).

## 5. Filter envelope IG00152 — IL/AL around the cutoff slider [S §4.1, §7.1, §13]

Output in volts added to the cutoff sum. The *rest* (sustain) level is the cutoff slider itself:

```
key-on:  start at −5 V·il  →(attack A)→  +5 V·al  →(decay D)→ 0 V   (held)
key-off: from current value →(release R)→ −5 V·il
```

IL only → AR shape; AL only → AD shape (test 5). Attack is an RC charge toward an overshoot
target `peak + 0.3·|peak − start|` that stops at the peak (analog feel, G§8.1 option B);
decay/release are exponential with `τ = T / 2.3` (T = time to 10 %). Retrigger starts from the
current value (no reset).

**Time law** [S ranges, D curve]: `T = Tmin · (Tmax/Tmin)^x`, x = slider position:
attack 2 ms…580 ms, decay 2 ms…8.75 s, release 2 ms…11 s. Ranges from the Arturia CS-80 V
manual §5.2.1.3, an emulation documented against the hardware (secondary source; replaced
the earlier 1 ms…1 s / 10 ms…10 s defaults on 2026-10-09).

**Long envelope mode** [A] (`env.long`, stored per patch; the same manual's "Long" mode, a
global setting there): the filter and VCA envelope ranges become attack 2 ms…10 s,
decay 2 ms…25 s, release 2 ms…40 s, for slow-swelling pads. Sustain time and the ring-mod
envelope are not affected.

## 6. VCA envelope IG00159 — ADSR [S]

Same time law; ranges A 2 ms–885 ms, D 2 ms–7.35 s, R 2 ms–11.5 s [S Arturia manual §5.2.1.4],
Long mode [A] as in §5; sustain `S = vegS` (linear 0..1). Attack is RC toward 1.3 with stop at 1.0; decay and release exponential (τ = T/2.3).
Release time while the **sustain pedal** is down: `T_R = T(vegR) + T_sustain` (doc 03 §8) —
same for the filter envelope release.

## 7. VCA and dynamics [S §4.1, §14.3]

```
pre   = vcfLevel · LP_out + sineLevel · sine
dyn   = (1 − 0.75·initLevel) + 0.75·initLevel·velocity + 0.5·afterLevel·pressure
gain  = VEG · dyn · level · kbdLevel · subVCA · lineMix
out   = gain · pre
```

`level` is linear (B10K slider into the IG00151 linear LI input) [S]. `kbdLevel = max(0, 1 + 0.8·kbdLevelOffset)`
(doc 03 §6). Velocity and pressure are per voice, 0..1, smoothed: velocity is latched at
note-on; pressure is smoothed with τ = 100 ms (the TWS A-output low-pass, 1 µF·100 kΩ) [S p.17].
A per-card VCA control offset (feedthrough) of up to −70 dB is added [D].

## 8. Per-card calibration and drift [S §7, §9.2; D numbers]

Seeded per card (16 cards, seed exposed in code) and scaled by the global **Drift** control
[A] (0 = perfect calibration, default 0.35):

| Quantity | Spread (at Drift = 1) |
| --- | --- |
| VCO scale error | ±0.15 % |
| VCO offset | ±0.6 Hz |
| Slow drift | Ornstein–Uhlenbeck, σ 0.25 Hz, τ 20 s |
| Saw pulse width/height | ±30 % |
| Filter cutoff scale | ±4 % |
| Filter Q scale | ±5 % |
| Envelope time scale | ±6 % |
| VCA feedthrough | −70…−60 dB |
| VCO cycle-to-cycle jitter | frequency × (1 + 5·10⁻⁴·j·g) per cycle, g Gaussian, j = card factor 0.7…1.3 [D] |

Jitter: each VCO cycle runs at a fresh random frequency factor drawn at the reset (TAE-style
period-to-period instability; plan 1.14). At the default Drift 0.35 the rms is ~0.3 cent.
A line at Level 0 or Mix 0 is not rendered (its VCO phase still runs); its −60 dB VCA
feedthrough is dropped with it.

## 9. Preset-row inversions (decoded factory data → slider position)

The factory matrices store row *voltages*. Conversion to slider position (doc 05):
- time rows (13–15, 18, 19, 21): `pos = 1 − V/10` (**higher voltage = shorter time** [I §14.5; tested
  in spec 05 §3: 62/74 plausible vs 33/74 for the opposite polarity, plan P-1]);
- resonance rows (8, 10): `pos = 1 − V/10` (VQ 10 V = Q 0.5 = no resonance) [S];
- all other rows: `pos = V/10`.
