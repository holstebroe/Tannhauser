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

- Phase accumulator `φ ∈ [0,1)`, increment `f/fs_os`. Saw = `2φ − 1` with PolyBLEP at reset.
- **Saw start pulse**: a short pulse added at each reset, width `w = 2 %` of the period
  (min 1.5 samples), height `+0.2`, both randomised ±30 % per card [D §14.6]. Rendered as a
  band-limited rectangular pulse (PolyBLEP on both edges).
- **Pulse** = comparator of the saw against the PW threshold, PolyBLEP on both edges.
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

Each filter is a 2-pole TPT state-variable filter (Zavalishin/Simper). HPF output feeds LPF.
A fixed one-pole low-pass at **7.6 kHz** follows the LPF (the extra real pole ωi of §6.2).
The input is softly saturated `x → 1.2·tanh(x/1.2)` (OTA input stage, mild) [D, option B later].

**Cutoff law (linear in Vf, keyboard-tracked)** [S IC data p.42: KV 0.25 V & Vf 5 V → 1 kHz]:

```
fc = 1000 Hz × (Vf / 5 V) × (f_key8 / 130.81 Hz) ,   f_key8 = key frequency at 8′ (no footage)
```

`fc` is floored at 20 Hz (the ω0 = 2π·23 Hz control floor) and capped at 0.45 × fs_os.
So at Vf = 10 V the cutoff is ~15.3 × the 8′ fundamental; tracking is 100 % and done by the
key voltage, not by footage.

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
attack 1 ms…1 s, decay 10 ms…10 s, release 10 ms…10 s.

## 6. VCA envelope IG00159 — ADSR [S]

Same time law and ranges (A 1 ms–1 s, D 10 ms–10 s, R 10 ms–10 s); sustain `S = vegS` (linear
0..1). Attack is RC toward 1.3 with stop at 1.0; decay and release exponential (τ = T/2.3).
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

## 9. Preset-row inversions (decoded factory data → slider position)

The factory matrices store row *voltages*. Conversion to slider position (doc 05):
- time rows (13–15, 18, 19, 21): `pos = 1 − V/10` (**higher voltage = shorter time** [I §14.5,
  see open issue P-1 in the plan: this does not fit the String/Brass presets]);
- resonance rows (8, 10): `pos = 1 − V/10` (VQ 10 V = Q 0.5 = no resonance) [S];
- all other rows: `pos = V/10`.
