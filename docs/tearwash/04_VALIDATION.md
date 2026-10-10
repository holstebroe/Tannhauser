# 04 — Validation

The reference is the original firmware running in an emulator (the *oracle*), not a written
description. Waveforms cannot be compared sample by sample (the 8080 reads DSP data and
branches on it, so the machine is chaotic in its input [R B:tools/README]); comparisons use
the acoustic metrics of §3.

## 1. The oracle

| Flavour | Oracle | Status |
| --- | --- | --- |
| 224XL | BlueBox `lexcore` (8080 SBC + DSP, bit-exact ARU, FPC, 224X emphasis) running the user's **224XL V8.21** ROMs | working (PLAN TW1.1) |
| 224X | BlueBox core with the V8.1 ROMs: needs the V8.1 entry points / program-select hook | open (TW1.4) |
| 224 | new machine model: 224 microword layout, 20 kHz, 16 K DMEM, V4.4 8080 I/O and remote head; same ARU/FPC | open (TW1.5) |

Setup (fetches BlueBox at a pinned commit into `build/ext/`, installs the ROM images from
`training/lex/` into `build/ext/roms/`, builds `tearwash_oracle`, renders the set):

```
tools/oracle/setup_bluebox.sh                      # → build/oracle/xl/{index.tsv, *.wav}
build/tearwash_calib build/oracle/xl --report docs/tearwash/reports/<name>.md --tsv build/<name>.tsv
```

`tearwash_oracle` boots a program, moves LARC sliders through the firmware's own handler,
forces option bits at load, settles 2 s, then renders the stimulus to a 4-channel float WAV
(DACs A–D, full scale 1.0 = ADC/DAC full scale) at 48 kHz. `index.tsv` records the program,
loop length, DSP rate, the requested settings and the slider registers read back from the
firmware. The calibration set is `tools/oracle/calib_xl.txt` (one case per line).

## 2. Stimuli (`src/tearwash/analysis/Stimuli.hpp`)

All start after 50 ms of silence; L and R identical unless suffixed L/R.

| Name | Signal | Used for |
| --- | --- | --- |
| `sweep`, `sweepL`, `sweepR` | exponential sweep 20 Hz–20 kHz, 3 s, −18 dBFS, 10 ms fades; deconvolved with the Farina inverse filter | impulse responses: every IR metric |
| `impulse`, `impulseL`, `impulseR` | one sample at 0.5 | cross-check only: an impulse rises only ≈ 40 dB above the 16-bit core's truncation floor, the sweep ≈ 75 dB |
| `burst` | white Gaussian noise, 0.3 s, −18 dBFS RMS | interrupted-noise decay, steady-state spectrum, Decay Optimisation |
| `sine1k` | 1 kHz, −12 dBFS peak, 1 s, 5 ms edges | modulation (Mode Enhancement) |

−18 dBFS leaves room for the 224X pre-emphasis (+12 dB at HF) before the ADC clips.

## 3. Metrics (`src/tearwash/analysis/Metrics.hpp`)

Every output channel is DC-blocked first (15 Hz, zero phase): the ARU's truncation leaves a DC
offset of a few LSB that the hardware's output transformers remove. The stereo pair is DAC A
(L) and DAC C (R); "mono" is their mean.

| Metric | Definition |
| --- | --- |
| Band RT | octave bands 125 Hz–8 kHz (4th-order Butterworth HP·LP, zero phase). IR: Schroeder backward integration up to the first point where the 20 ms level is within 5 dB of the noise floor (mean energy of the last 10 %); T20 (−5…−25 dB) if ≥ 30 dB range, T30 (−5…−35) if ≥ 40 dB; RT = T30, else T20. Burst: block levels after the burst, same fits |
| EDT | 0…−10 dB of the EDC, broadband (80 Hz–8 kHz) |
| Onset | first sample after the excitation where the broadband IR reaches −20 dB re its peak |
| NED | Abel–Huang normalised echo density, 20 ms window, 5 ms hop, from the onset (Gaussian noise = 1); **NED mean** over 50–300 ms; **mixing time** = first time the 30 ms average reaches 0.9 |
| C50 | early (0–50 ms after onset) to late energy, dB |
| Spectrum | third-octave levels 63 Hz–12.5 kHz of the IR from onset + 50 ms for 0.5 s (burst: last 150 ms of the excitation), normalised to the 250 Hz–4 kHz mean; **HF edge** = last band above 1 kHz before the level first drops 10 dB below the 1 kHz band |
| IACC | max normalised cross-correlation of L and R (100 Hz–8 kHz) within ±1 ms, 80–500 ms after onset |
| Modulation | sine: energy 6–150 Hz away from 1 kHz over energy within ±6 Hz, in the tail 50–550 ms after the tone stops (dB) |
| Gain | output energy over input energy, dB |

## 4. Scores and acceptance

`tearwash_calib` prints, per candidate, over the 22 factory programs:

| Score | Definition | 224XL target | 224 / 224X target (once their oracles exist) |
| --- | --- | --- | --- |
| Band RT error | mean over programs of mean \|RT_c/RT_o − 1\| over the 7 bands | ≤ 5 % | ≤ 5 % |
| EDT error | mean \|EDT_c/EDT_o − 1\| | ≤ 10 % | ≤ 10 % |
| Tail spectrum error | RMS dB difference 100 Hz–10 kHz | ≤ 1.0 dB | ≤ 1.0 dB |
| Echo density error | RMS NED difference over the first 300 ms | ≤ 0.05 | ≤ 0.05 |
| Stereo correlation error | mean \|IACC_c − IACC_o\| | ≤ 0.05 | ≤ 0.05 |
| Onset | per program | ±1 ms | ±1 ms |
| Modulation | per sine case | ±2 dB | ±2 dB |
| Gain | per program, absolute (faithful flavours keep the hardware's level) | ±1 dB | ±1 dB |

Sweeps (decay law, crossover, treble, depth, diffusion, predelay) must match row by row with
the same tolerances; candidates without a control are fitted where they can be (the 225 plate)
and the residual is reported, not scored.

## 5. Automated tests (planned `tearwash_engine_test`; status in PLAN.md)

| ID | Test | Pass |
| --- | --- | --- |
| W1 | ARU multiply-accumulate vs the ROM self-test vectors (read from the user's ROMs; skipped if absent) | all vectors bit-exact |
| W2 | Allpass and one-pole blocks on the Core | match the hardware form of 02 §2 to the LSB |
| W3 | FPC input/output quantisation | step 16/8/4/2 per range; output normalisation ≤ 3 shifts |
| W4 | Silence in → silence out after the tail (DC-blocked), every program | < −120 dBFS, no limit cycle above −100 dBFS |
| W5 | Robustness: full-scale noise and square into every program, every flavour | finite, no NaN, overflow saturates |
| W6 | Sample-rate invariance 44.1 / 48 / 96 kHz | band RT within 2 %, spectrum within 0.5 dB |
| W7 | Block-size invariance (1, 64, 512 samples) | bit-identical |
| W8 | CPU, one instance, 224XL CONCERT HALL at 48 kHz | real-time factor < 0.05 |
| W9 | Calibration scores against the stored oracle metrics (`reports/*.tsv`) | §4 targets |
| W10 | Tannhäuser: T12 / T16 with the new reverb | as the CS-80 spec |

## 6. Listening

After the numbers pass: drums into plates, vocals into Vocal Plate / PLATE, strings and CS-80
pads into the halls, at factory settings and at long decays; A/B against oracle renders of the
same files (`tearwash_oracle` accepts any stimulus name; add a `file:` stimulus when needed).
