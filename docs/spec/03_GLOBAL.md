# 03 — Global, performance and bus sections

Paddles on the hardware run "backwards" (fully up = off); in the plugin every control is
"up/right = more" and the reversal is only a visual detail [S §4].

## 1. Key assignment (KAS: YM26600/YM26700) [S §6.3, §13]

- 8 voices, each = line I + line II on the same key.
- Assignment: a re-struck key that still owns a voice reuses it; otherwise the first free voice
  in round-robin order; if none is free, steal the voice released longest ago; if all are held,
  steal the oldest held note. A stolen voice is not reset (analog), its envelopes retrigger
  from their current value. [D details]
- Velocity is MIDI velocity (touch extraction rule unknown, §10) [D]; aftertouch is polyphonic
  key pressure, CLAP pressure expression, or channel pressure applied to all held voices [A].

## 2. Portamento / glissando (polyphonic) [S YM26700 p.6, §4.3]

- Each newly struck voice glides **from the last played key** (global, monosynth-style but
  polyphonic) to its own key.
- The YM26700 is clocked by the porta clock PC, period `T = 1.4 ms · 1000^x` (x = Porta time
  slider; x = 0 is **off**). [S range]
- **Portamento**: the key voltage moves toward the target at a constant rate of one semitone
  per clock period (linear in log pitch), followed by a simple low-pass (τ = 5 ms) [I].
- **Glissando**: the key voltage steps one semitone per period, same low-pass smoothing.

## 3. Sub-oscillator (global LFO, IG00150) [S SUB board, Panel 2]

- Waveforms (FUNCTION, 6 positions): sine, saw up, saw down, square, sample & hold (random
  step each cycle), noise (smoothed random, ~ speed rate). [S labels; the hardware's 6th is an
  external input — replaced by smoothed noise [A]]
- Speed: `f = 0.5 Hz · 200^x` (0.5…100 Hz, reaching the audio range) [S range: Arturia CS-80 V
  manual §5.2.2.3; D curve]. Was 0.1…200 Hz [D] before 2026-10-09.
- Depths (all squared taper): VCO `±1 octave · d²` exponential; VCF `±5 V · d`; VCA tremolo
  `gain × (1 − d·(0.5 − 0.5·lfo))`.
- **Touch Response** (aftertouch): Speed raises the sub-osc rate by `+0.4·touchSpeed·Pmax` in
  slider units (Pmax = highest pressure of all voices: single SP output) [S]; VCO and VCF add
  per voice `touchVco·pressure` and `touchVcf·pressure` to the respective depths [S TRG3].
- The sub-osc is free-running and shared by all voices.

## 4. PWM LFOs [S SUB board]

One sine LFO per *line* (two in total, shared by the 8 cards of a line), rate
`f = 0.1 Hz · 1270^x` (0.1…127 Hz) [S range: Arturia CS-80 V manual §5.2.1.1; D curve; was
0.1…25 Hz], depth = `pwmDepth` (doc 02 §2).

## 5. Initial pitch bend (scoop) [S §4.2, TRG4]

At note-on the voice starts `−2 semitones · touchPitchBend · velocity` below its target and
returns exponentially with a constant τ = 60 ms (same speed for every depth) [D depth/τ].

## 6. Keyboard control (KBC1/KBC2) [S]

Four bipolar controls: Brilliance Low/High, Level Low/High. For key position
`t = (note − 36)/60` (C2..C7 MIDI 36..96, clamped) the offset is linear on each half,
neutral at the centre: `t < 0.5: low·(1 − 2t)`, else `high·(2t − 1)`. Brilliance offset is
`±3 V` into both filters' mods; level offset feeds `kbdLevel` (doc 02 §7). Smoothed ~1 ms.

## 7. Ribbon, bend, master controls

- **Ribbon** [S §4.2]: pitch is relative to the first touch point; full ribbon width =
  ±1 octave from there (range unknown, D). On the CS-80 the pitch returns to 0 on release.
  **Ribbon Hold** [A] (`ribbon.hold`, default on, not stored): while the ribbon is touched
  (`ribbon.touch`, set by the GUI) every sounding voice follows it from the bend it already
  has; on release the voices keep their bend until they are re-struck or stolen. A note struck
  later starts unbent (one struck during a touch follows the ribbon from that moment). Hold off
  = the hardware's return to pitch. Panel: the lit "H" key left of the ribbon. A ribbon value written without a touch (host automation)
  bends every voice globally and is not held. Bends are smoothed ~2 ms. Test T22.
  GUI: a drag strip above the keyboard area. MIDI pitch bend is an added equivalent with
  **Bend Range** (default 2 semitones) [A].
- **Detune** (line II), **Mix** (line I ↔ II crossfade: `I·(1−mix)`, `II·mix`, with a
  1.0 centre boost so mix 0.5 ≈ both full: gains `min(1, 2(1−mix))`, `min(1, 2·mix)`) [D],
  **Brilliance** (±4 V on both lines' cutoffs), **Resonance** (bipolar −1..+1, adds to or
  subtracts from both resonances, `−10 V · res` on VQ, doc 02 §4) [S bipolar range: Arturia
  manual §5.2.2.6; was 0..1 before 2026-10-09, stored values keep their meaning].

## 8. Sustain [S §4.3, §7.1]

- Pedal: CC64 or the panel sustain switch.
- **Sustain mode I**: each released note fades independently with release `T_R + T_sus`.
- **Sustain mode II**: only the last note/chord carries the sustain; a new note-on
  immediately silences voices that are fading (5 ms fast release).
- **Sustain time** `T_sus = 10 ms · 1000^x` (to 10 s), added to the VCA (and VCF) release
  while the pedal is down. Released notes always decay (never infinite).

## 9. Ring modulator (PRA board: µA796 + IG00150 + IG00159/IG00151) [S]

Acts on the **mono sum**. Its own sine carrier and its own AD envelope:

```
env:  AD, attack 3 ms·(530 ms/3 ms)^attack, decay 7 ms·(4.5 s/7 ms)^decay; retriggers only
      when a key goes down while no key was held (monophonic, "after all keys are released") [S]
f_rm = 0.25 Hz + 204.75 Hz · (speed + depth · env)      (clamped sum 0..2; unclipped by speed)
out  = bus · (1 − mod) + bus · carrier · mod                  (mod = Modulation slider)
```

Ranges 3–530 ms, 7 ms–4.5 s and 0.25–205 Hz from the Arturia CS-80 V manual §5.2.2.2 [S];
the law stays linear in volts (IG00150 ~200 Hz at 10 V) with the 0.25 Hz floor as an offset
[I]. Mild carrier leakage (−50 dB) and balanced-modulator softness `tanh` on the product [D B-option].

## 10. Expression / wah

EXP pedal: CC11 → `expression` (0..1, linear gain). The separate wah circuit is not
implemented yet (plan WP7).

## 11. Chorus / tremolo (OE1/OE2: MN3001 BBD) [S topology, D values: OE pages unread]

- Chorus: mono in → BBD-like modulated delay with two taps, each tap band-limited (8 kHz
  2-pole low-pass before and after), BBD noise floor −80 dB. Base delays 7 ms and 11 ms; LFO
  `f = 0.2 Hz · 40^speed` mixed with a 6.0 Hz vibrato component at 0.15 of the depth;
  depth `±(0.2…4 ms)` set by Depth. Tap A moves with the LFO, tap B against it;
  `L = dry·(1 − 0.3c) + 0.7c·tapA`, `R = dry·(1 − 0.3c) + 0.7c·tapB` (c = chorus on, 20 ms fade).
- Tremolo: stereo antiphase amplitude modulation, rate `0.5 Hz · 20^speed`, depth 0..1.
- With both off: L = R = mono (as on the hardware).

## 12. Reverb [A] (README: "Lexicon 224 emulation" for space pads)

Plate/hall algorithm in the Dattorro (1997) topology with modulated tank allpasses:
pre-delay 0–150 ms, decay 0.2–0.98 feedback, damping one-pole in the tank, input diffusion.
Mix 0..1 (default 0.15 in pad presets, 0 in the factory tones). A true 224 algorithm study is
plan WP8.

## 13. Output

**Patch Gain** [A] (`gain`, dB, stored in presets) scales the mono bus after decimation, before
chorus/reverb; it is the per-preset loudness trim (spec 05 §4).

`L,R × 1.6·volume²`, then a soft clip `tanh` (the "hot" post-mix VCA, §7.1 unverified) [D].
The voice bus is scaled by 0.15 before the ring modulator.
