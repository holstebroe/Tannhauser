# 04 — Parameters

`src/core/Params.hpp/.cpp` is the single source of truth (id, key, name, range, default,
flags). This table mirrors it; keep both in sync. CLAP ids are stable: **never renumber**,
only append. The state is saved as `key=value` text, so keys must never change either.

Flags: **S** = stored in software presets (patch); **P** = performance-only (not stored);
**T** = stepped. All continuous controls are 0..1 slider positions unless a range is given.

## Per-line parameters (ids 0–26 line I `l1.*`, ids 27–53 line II `l2.*`, same order)

| # | Key | Name | Range / default | Flags | CS-80 control (row) |
| --- | --- | --- | --- | --- | --- |
| 0 | feet | Feet | 0..5 = 16′,8′,5⅓′,4′,2⅔′,2′; def 1 (8′) | S T | Feet lever |
| 1 | pwmSpeed | PWM Speed | 0.232 (0.5 Hz; 0.1–127 Hz) | S | VR1 (1) |
| 2 | pwmDepth | PWM | 0 | S | VR2 (2) |
| 3 | pw | PW | 0 | S | VR3 (3) |
| 4 | square | Square | 0/1, def 0 | S T | SW1 (4) |
| 5 | saw | Saw | 0/1, def 1 | S T | SW2 (5) |
| 6 | noise | Noise | 0 | S | VR4 (6) |
| 7 | hpf | HPF | 0 | S | VR5 (7) |
| 8 | resH | Res H | 0 | S | VR6 (8, inverted) |
| 9 | lpf | LPF | 0.7 | S | VR7 (9) |
| 10 | resL | Res L | 0.2 | S | VR8 (10, inverted) |
| 11 | il | IL | 0 | S | VR9 (11) |
| 12 | al | AL | 0.3 | S | VR10 (12) |
| 13 | fegA | VCF Attack | 0.243 (8 ms; 2–580 ms) | S | VR11 (13, inverted) |
| 14 | fegD | VCF Decay | 0.604 (0.32 s; 2 ms–8.75 s) | S | VR12 (14, inverted) |
| 15 | fegR | VCF Release | 0.508 (0.16 s; 2 ms–11 s) | S | VR13 (15, inverted) |
| 16 | vcfLevel | VCF Level | 1.0 | S | VR14 (16) |
| 17 | sine | Sine | 0 | S | VR15 (17) — wave symbol = sine level [I: row 17 → M pin LP1, the VCA after the waveshaper sine] |
| 18 | vegA | VCA Attack | 0.056 (2.8 ms; 2–885 ms) | S | VR16 (18, inverted) |
| 19 | vegD | VCA Decay | 0.617 (0.32 s; 2 ms–7.35 s) | S | VR17 (19, inverted) |
| 20 | vegS | VCA Sustain | 0.8 | S | VR18 (20) |
| 21 | vegR | VCA Release | 0.505 (0.16 s; 2 ms–11.5 s) | S | VR19 (21, inverted) |
| 22 | level | Level | 0.8 (line II def 0.8) | S | VR20 (22) |
| 23 | initBrill | Init Brilliance | 0.3 | S | VR21 (23) |
| 24 | initLevel | Init Level | 0.5 | S | VR22 (24) |
| 25 | afterBrill | After Brilliance | 0.3 | S | VR23 (25) |
| 26 | afterLevel | After Level | 0.3 | S | VR24 (26) |

## Global parameters (ids from 54)

| # | Key | Name | Range / default | Flags | Hardware |
| --- | --- | --- | --- | --- | --- |
| 54 | volume | Volume | 0.7 | P | Panel 2 VR23 |
| 55 | pitch | Pitch | −1..+1 semitone, 0 | P | Panel 2 VR1 |
| 56 | detune | Detune | 0 | S | Panel 2 VR2 |
| 57 | mix | Mix | 0.5 | S | VR12 |
| 58 | brilliance | Brilliance | −1..+1, 0 | S | VR13 |
| 59 | resonance | Resonance | −1..+1, 0 | S | VR14 |
| 60 | sub.func | Sub Osc Function | 0..5 sine, saw up, saw down, square, S&H, noise; 0 | S T | SW1 |
| 61 | sub.speed | Sub Osc Speed | 0.342 (3.1 Hz; 0.5–100 Hz) | S | VR8 |
| 62 | sub.vco | Sub Osc VCO | 0 | S | VR9 |
| 63 | sub.vcf | Sub Osc VCF | 0 | S | VR10 |
| 64 | sub.vca | Sub Osc VCA | 0 | S | VR11 |
| 65 | touch.bend | Touch Pitch Bend | 0 | S | VR15 |
| 66 | touch.speed | Touch Sub Speed | 0 | S | VR16 |
| 67 | touch.vco | Touch Sub VCO | 0 | S | VR17 |
| 68 | touch.vcf | Touch Sub VCF | 0 | S | VR18 |
| 69 | kbd.brillLow | Kbd Brilliance Low | −1..+1, 0 | S | VR19 |
| 70 | kbd.brillHigh | Kbd Brilliance High | −1..+1, 0 | S | VR20 |
| 71 | kbd.levelLow | Kbd Level Low | −1..+1, 0 | S | VR21 |
| 72 | kbd.levelHigh | Kbd Level High | −1..+1, 0 | S | VR22 |
| 73 | rm.attack | Ring Mod Attack | 0 (3–530 ms) | S | PRA VR3 |
| 74 | rm.decay | Ring Mod Decay | 0.589 (0.32 s; 7 ms–4.5 s) | S | VR4 |
| 75 | rm.depth | Ring Mod Depth | 0 | S | VR5 |
| 76 | rm.speed | Ring Mod Speed | 0.3 (61.7 Hz; 0.25–205 Hz) | S | VR6 |
| 77 | rm.mod | Ring Mod Modulation | 0 | S | VR7 |
| 78 | sus.mode | Sustain Mode | 0 = I, 1 = II | S T | Panel 3 SW3 |
| 79 | sus.time | Sustain Time | 0.4 | S | VR1 |
| 80 | sus.pedal | Sustain | 0/1 | P T | SW1 / CC64 |
| 81 | porta.mode | Porta/Gliss | 0 = portamento, 1 = glissando | S T | SW4 |
| 82 | porta.time | Porta Time | 0 (= off) | S | VR2 |
| 83 | chorus | Chorus | 0/1 | S T | SW6 |
| 84 | tremolo | Tremolo | 0/1 | S T | SW5 |
| 85 | fx.speed | Chorus/Trem Speed | 0.3 | S | VR3 |
| 86 | fx.depth | Chorus/Trem Depth | 0.5 | S | VR4 |
| 87 | ribbon | Ribbon | −1..+1, 0 | P | Ribbon |
| 88 | bendRange | Bend Range [A] | 0..12 semitones, 2 | P T | — |
| 89 | expression | Expression | 1.0 | P | EXP pedal / CC11 |
| 90 | drift | Drift [A] | 0.35 | P | — |
| 91 | rev.mix | Reverb Mix [A] | 0 | S | — |
| 92 | rev.decay | Reverb Decay [A] | 0.6 | S | — |
| 93 | rev.tone | Reverb Tone [A] | 0.6 | S | — |
| 94 | rev.predelay | Reverb Pre-delay [A] | 0.2 | S | — |
| 95 | gain | Patch Gain [A] | −24..+24 dB, 0 | S | — (per-preset loudness trim, spec 05 §4) |
| 96 | env.long | Long Envelopes [A] | 0/1, 0 | S T | — (spec 02 §5: filter/VCA envelope times to 10/25/40 s) |
| 97 | os.4x | 4x Oversampling [A] | 0/1, 0 | P T | — (spec 01 §3; host/automation only, no panel control) |

Count: 98 (`PARAM_COUNT`).

Time and rate ranges follow the Arturia CS-80 V manual (spec 02/03). Defaults and library
presets were remapped on 2026-10-09 so they keep the times they had under the earlier laws.

## MIDI

| MIDI | Target |
| --- | --- |
| Note on/off, velocity | key assigner, velocity |
| Poly pressure (0xA0) / CLAP pressure expression | per-voice aftertouch |
| Channel pressure (0xD0) | all held voices' aftertouch [A] |
| Pitch bend | bend, ± `bendRange` [A] |
| CC1 mod wheel | added to Sub Osc VCO depth (0..+0.5) [A] |
| CC11 | expression |
| CC64 | sustain pedal |
| CC 120/123 | all sound/notes off |

## Value display

Times are shown in ms/s via the doc 02/03 time laws (envelope times follow the Long mode),
rates (PWM, sub-osc, ring-mod speed) in Hz, cutoffs in volts, feet as 16′…2′, switches
as On/Off, bipolar controls with sign.
