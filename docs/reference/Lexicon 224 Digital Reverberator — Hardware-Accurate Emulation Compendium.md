# Lexicon 224 Digital Reverberator — Hardware-Accurate Emulation Compendium

Oct 4, 2026 · @Søren

A circuit- and algorithm-informed reference for emulating the Lexicon 224 digital reverberator, built to the project's ten-section standard.

## Purpose, scope and confidence legend

The Lexicon 224 is a 1978–79 stereo digital reverberator built from discrete 74-series logic, not a DSP chip, so an accurate emulation must model the algorithms, the 20 kHz (224) or higher (224X) sample rate, the 12-bit floating-gain converters and the transformers, not only the reverb tail.

Unlike the three pedal compendiums in this project, there is no analog gain-stage circuit to solve. The emulation problem is a *signal-processing structure* (all-pass diffusers, delay lines, damping filters, modulation) plus a *front end and back end* (transformers, anti-alias filters, 12-bit gain-ranged converters). The project's clipping-solver material does not apply; the reusable parts are the confidence legend, the measurement-first workflow, the test-matrix format, and the parameter-smoothing and DC-blocking practice from the synth compendiums.

**Confidence legend** (same grades as the project's other compendiums): *sourced* = stated by a page I actually opened; *inferred* = deduced from sourced facts or from the same design lineage; *approx.* = an estimate needing calibration; *unknown* = no evidence found. Where I name a section as *unverified*, no primary source (manual, schematic, patent, interview) was read.

**Scope limits.** I did not obtain the 224 owner's manual, the service manual or the firmware images, so exact algorithm topologies, delay lengths and coefficient tables are not sourced. The strongest primary-adjacent sources read were a repair engineer's card-by-card description (Benden Sound Technology), Jon Dattorro's AES paper (which describes a Griesinger-style plate, explicitly not the 224 code), and a Sound On Sound review of the licensed UAD port. Section 8 is therefore a design proposal built on those, not a reconstruction of the original program ROMs.

**Update with the service manual.** A 113-page Lexicon 224 Service Manual (1 July 1980) was supplied after the first draft. It supersedes the "unknown" and "approx." entries it covers; section 11 lists what changed and where, and the affected earlier cells are edited in place.

**Start here to implement.** Sections 12 and 13 supersede earlier sections where they disagree (fractional delay, program data, slider laws). Section 13 is the build guide: what is known, how to use the data pack, original presets, and the one remaining blocker (microword opcode decode).

## 1. History, production revisions and prices

The 224 was unveiled at the 1978 AES show and sold in three generations, 224 (20 kHz), 224X (higher sample rate, doubled memory) and 224XL (new remote), told apart by cards and firmware version, not by any serial-number range I could find.

**Origins.** David Griesinger, a physicist and classical recording engineer, built an early digital reverb prototype; seeing the EMT 250 pushed him to add a microcomputer, and Lexicon bought the design and hired him ([Mix](https://www.mixonline.com/technology/1978-lexicon-224-digital-reverb-383667), search snippet only; Griesinger's [own page](http://www.davidgriesinger.com/acoustics_today/) confirms he developed one of the first digital reverbs that became the 224). His idea of a separate console-top controller shaped the product. The EMT 250 (Barry Blesser and Karl-Otto Bader's design) reached the US from 1977 at a 32 kHz sample rate with 8K words of memory ([Dattorro, AES 1997](https://ccrma.stanford.edu/~dattorro/EffectDesignPart1.pdf)).

| Revision | Date | Sample rate / bandwidth | Memory | Firmware, cards, remote | Confidence |
| --- | --- | --- | --- | --- | --- |
| 224 (original) | Unveiled 1978; Dattorro says introduced 1979, so first shipments likely 1979 (approx.) | 20 kHz; audio bandwidth about 8 kHz | 16K words × 16 bit, 4116 DRAM | Seven cards in an 8-slot Multibus I frame; firmware up to 4.3; basic remote head on a parallel link | Sourced ([Benden](http://www.lenham.clara.net/sound/lexicon224p1.html), [Dattorro](https://ccrma.stanford.edu/~dattorro/EffectDesignPart1.pdf)) |
| 224 with NVS card | Date not found | Unchanged | Unchanged | Eighth card (NVS): battery-backed RAM on 3 AAA NiCd cells, more EPROM, user patches and more factory patches; firmware 4.4. UA's plug-in is built on 4.4 | Sourced (Benden, [UA](https://help.uaudio.com/hc/en-us/articles/4419497193492-Lexicon-224-Digital-Reverb-Manual)) |
| 224X | 1983 (secondary sources; one retailer page gives both 1983 and 1985) | 34.5 kHz quoted second-hand; Benden says bandwidth doubled to about 16 kHz; spec 20 Hz–15 kHz ±1.5 dB | 32K words (new DMEM and T&C cards) | AIN/AOUT filter cutoffs raised; firmware up to 8.1; same remote head. Retailer spec: 18 programs, 59 preset variations | Sample rate: approx.; the rest sourced ([Benden](http://www.lenham.clara.net/sound/lexicon224p1.html), [Vintage Digital](https://www.vintagedigital.com.au/lexicon-224x-digital-reverberator/), [Little Devil](https://littledevilstudios.com.au/2015/02/lexicon-224-digital-reverb/)) |
| 224XL | 1984 | As 224X | As 224X | LARC remote (serial link, tape interface, alphanumeric LED); new header board; firmware 8.1A–8.2.1. Schematics call the LARC "LURCH" | Sourced (Benden, [Vintage Digital](https://www.vintagedigital.com.au/lexicon-224xl/)) |

**Serial numbers.** No source I read gives serial ranges for any boundary above. Do not encode serial-dependent behaviour; model by firmware version (4.4 is the best-documented target).

**Serial boundary found (service manual).** Software version 2 (adds Decay Optimisation) needs a hardware ECO on the T&C card, pin 1 of the 74LS08 at IC36 (a 500 ns processor pause), and units below serial 2102 may also need a protect-circuit change. Treat serial 2102 as the earliest documented revision boundary (sourced, details in 11.4).

**Unverified revision claims.** One forum post and a blog say the 224X/XL moved from Burr-Brown ADC80/DAC80 to ADC800/DAC800 converters ([Gearspace](https://gearspace.com/threads/lexicon-224xl.992645/), [Little Devil](https://littledevilstudios.com.au/2015/02/lexicon-224-digital-reverb/)); the Little Devil page attributes the 34.5 kHz figure to Valhalla DSP's Sean Costello. Treat both as unverified. Benden says some engineers prefer the earlier firmware's algorithms over the later ones.

**End of line.** Lexicon released the 480L in 1986 as the 224XL's successor ([Wikipedia](<https://en.wikipedia.org/wiki/Lexicon_(company)>), snippet only).

**Prices.**

| Item | Price | Source and caveat |
| --- | --- | --- |
| 224, 1978 list | $7,500 with two programs or $7,900 with four | [Mix](https://www.mixonline.com/technology/1978-lexicon-224-digital-reverb-383667); another blog swaps this to four and eight programs, so the program counts conflict |
| EMT 250, 1970s | About double the 224; $20,000 quoted | Mix; [Gearspace](https://gearspace.com/threads/lexicon-224xl.992645/) |
| Used 224/XL | Not established | One archive page quotes $8,000–15,000 for 2025 but also contains errors (Section 9); do not rely on it |
| UAD plug-in, 2011 | $349 | [Sound On Sound](https://www.soundonsound.com/reviews/ua-lexicon-224) |
| Arturia Rev LX-24, 2023 | $99 list, $69 intro | [KVR](https://www.kvraudio.com/news-print.php?id=57525) |

## 2. Anecdotes and famous uses

No record credit below is confirmed by a liner note or a first-hand engineer interview that I read; most come from Universal Audio marketing, so treat each as *marketing-level* unless the status says otherwise.

| Claim | Source | Status |
| --- | --- | --- |
| Peter Gabriel's "Sledgehammer" session had a 224 among its outboard: EMT plate, AMS 1580, RMX 16, Delta Labs DL2, 224 and a Quantec Room Simulator | [Sound On Sound, Classic Tracks](https://www.soundonsound.com/techniques/classic-tracks-peter-gabriel-sledgehammer) (search snippet) | Sourced as a gear list; the 224 was one of several reverbs, not the only one |
| Kevin Killen assisted and did extra engineering on U2's *The Unforgettable Fire* (1984) | Same article | Sourced |
| "Nearly every track" of *The Unforgettable Fire* is bathed in the 224 | [UA blog](https://www.uaudio.com/blogs/ua/everything-you-need-to-know-about-reverb) (snippet) | Marketing-level, unverified; "nearly every track" is not backed by a credit list |
| Talking Heads *Remain In Light*, Grandmaster Flash *The Message*, Kate Bush *Hounds of Love*, Peter Gabriel *So* used the 224 | [UA manual](https://help.uaudio.com/hc/en-us/articles/4419497193492-Lexicon-224-Digital-Reverb-Manual), UA blog | Marketing-level, unverified per track |
| Vangelis pioneered very long decays on the 224, most famously on the *Blade Runner* soundtrack | [Gearspace](https://gearspace.com/gear/lexicon/224), [Synthtopia](https://www.synthtopia.com/content/2023/04/06/arturia-rev-lx-24-copies-classic-blade-runner-reverb/), UA manual (all snippets or promotional) | Widely repeated, no primary source read; unverified |
| Pink Floyd's *The Final Cut* (1983) features the 224 | [Vintage Digital](https://vintagedigital.com.au/?p=143805) (snippet) | Unverified |
| Factory-style presets by Chuck Zwicky (Prince, Jeff Beck), E.T. Thorngren (Talking Heads, Bob Marley), Kevin Killen (U2, Peter Gabriel), David Isaac and Eli Janney ship with the UAD plug-in | [Danny Chesnut page for UA](https://www.dannychesnut.com/Recording/Lexicon/224/index.html) | Sourced that these engineers contributed presets; this does not prove which records used the 224 |
| The 224's "Concert Hall A" is well respected, and its plates are heard on 1980s drums | [Sound On Sound review](https://www.soundonsound.com/reviews/ua-lexicon-224) | One reviewer's opinion |
| The Ensoniq ESP2 plate topology (later published by Dattorro) began as a reverse-engineered 224 | Sean Costello forum post, quoted on [Freeverb3](https://freeverb3-vst.sourceforge.io/tips/reverb.shtml) | Hearsay ("according to another Ensoniq engineer"); unverified. Dattorro's paper itself says only "in the style of Griesinger" |
| Benden's repair bench A/B: the 224 sounded more spacious and less artificial than a Yamaha piano's reverb and a software reverb | [Benden](http://www.lenham.clara.net/sound/lexicon224p2.html) | One informal listening test |

**Why it sounds the way it does (opinion, not measurement).** Benden rules out the digital logic, since the algorithm fully defines it, and suggests the 12-bit successive-approximation converters and the input and output transformers as the source of the "mojo" ([Benden](http://www.lenham.clara.net/sound/lexicon224p2.html)). This is a hypothesis that Section 9's tests can check by turning each block on and off.

**Vocabulary.** Griesinger's "pink click" is a click source with a pink spectrum, used to judge diffusion settings ([Dattorro](https://ccrma.stanford.edu/~dattorro/EffectDesignPart1.pdf)).

## 3. Interface: every control

The v4.4 remote head has nine programs, six sliders, a handful of buttons and a 3-character display, and almost every range and unit below comes from Universal Audio's manual for its licensed port, which mirrors the hardware; I did not read the original owner's manual, so tapers are mostly unknown.

The table below describes the v4.4 plug-in port. The original 1979–80 hardware controls (8 programs, IMMED / SET / CALL / SHIFT, registers A–D, six sliders read as 8-bit codes, three-digit display) are in 11.3 and drawn in 11.5.

Sources: [UA manual](https://help.uaudio.com/hc/en-us/articles/4419497193492-Lexicon-224-Digital-Reverb-Manual) unless stated. *Plug-in-only* items are listed separately at the end and are not faithful behaviour.

| Control | Range / positions | Units | Taper and steps | How it is stored or quantised | Confidence |
| --- | --- | --- | --- | --- | --- |
| Program | 1–8 reverb, 9 chorus (shift + any program button) | none | Momentary buttons, no latch | Selecting a program loads its ROM algorithm and factory defaults, overwriting current settings unless Immed is on | Sourced |
| Bass (decay below Crossover) | 0.6 s to 70 s | seconds | Unknown; a 100× span suggests a log or table law (inferred) | Displayed in seconds; actual decay may differ from the number | Range sourced, taper unknown |
| Mid (decay above Crossover) | 0.6 s to 70 s | seconds | As Bass | Same; despite the name it governs all frequencies above Crossover | Range sourced, taper unknown |
| Crossover | 100 Hz to 10.9 kHz | Hz / kHz | Unknown (log or table, inferred) | Display switches unit; no effect if Bass equals Mid | Range sourced |
| Treble Decay | 100 Hz to 10.9 kHz | Hz / kHz | Unknown | Frequency above which decay is "very rapid"; sets amount of highs, while Mid sets their time | Range sourced |
| Depth | 0 to 71 | arbitrary integer | Unknown; 72 values is not a power of two, so likely a table index (inferred) | Not available in Chorus | Sourced |
| Predelay | Per program, in the table below | ms | Whole numbers; another retailer page says 1 ms steps | Range changes with program | Sourced; step size from a secondary page |
| Diffusion (Shift + Depth on hardware) | 0 to 63 | arbitrary integer | Non-monotonic: fastest density build near 32–37; above 40 can sound less dense | Inc/dec buttons; the first click only shows the value; held button repeats; fixed in P4 | Sourced |
| Immed | on / off, default off | none | Latching state | When on, Bass, Mid, Crossover, Treble Decay, Depth, Predelay, Diffusion, Mode Enh, Pitch Shift, Decay Opt and Rear Outs survive a program change | Sourced |
| Mode Enh enable | on (default) / off | none | Button | Enabling or toggling resets the algorithm, so it kills the tail | Sourced |
| Mode Enh amount | 1 to 16 | steps | Lower = stronger effect | Time between delay-line updates | Sourced |
| Mode Enh pitch shift | 1 to 16 | steps | Higher = stronger effect | Size of each delay-line update step; hidden on the plug-in, buried on hardware | Sourced |
| Decay Opt enable and amount | on (default) / off; 1 to 16 | steps | Lower = more prominent | Reduces diffusion and colour as input level rises; absent in P8 and P9 | Sourced |
| Rear Outs | off / on | none | Switch | Selects outputs B and D instead of A and C; in P2, P5, P8, P9 it just swaps left and right | Sourced |
| Display | 3-digit number, unit LEDs | s, ms, Hz, kHz | Shows an edit for 3.5 s, then an average decay time from "approximations designed by the original Lexicon engineers" | Display time is fixed on hardware | Sourced |
| Input meters and Overflow LED | five LEDs at −24, −18, −12, −6, 0 dB; 0 dB LED = input clipping | dB | Stepped | Meters read the A/D input; Overflow means arithmetic overflow in the processor | Sourced |
| Chorus sliders (P9) | Four sliders set the gain of four stereo voice pairs | linear fader | Linear, default about halfway = 6 dB below maximum | Bass, Mid, Crossover and Treble Decay are unavailable | Sourced |
| I/O | 2 inputs (L, R), 4 outputs A–D; A and C are the stereo pair | none | n/a | Dry signal is not passed through the 224 | Sourced |

**Program matrix** (v4.4; seven unique algorithms, because P1 = P3 and P2 = P5).

| Program | Predelay range (ms) | Input | Notes |
| --- | --- | --- | --- |
| 1 Small Concert Hall B | 24–152 | Stereo | Same algorithm as P3; best at 1.5–5 s |
| 2 Vocal Plate | 0–107 | Stereo | Same algorithm as P5, slightly different inherent diffusion |
| 3 Large Concert Hall B | 24–152 | Stereo | Same algorithm as P1; low density |
| 4 Acoustic Chamber | 25–255 | Mono | Diffusion fixed; best at 2–5 s; most chamber-like at Depth 0 |
| 5 Percussion Plate A | 0–107 | Stereo | High initial density |
| 6 Small Concert Hall A | 24–152 | Stereo | Brighter than P1; manual suggests about +3 dB EQ below 200 Hz on the return |
| 7 Room A | 24–255 | Stereo | Especially wide output with stereo input |
| 8 Constant Density Plate A | 5–185 | Inputs summed to mono | No Decay Opt; density does not build |
| 9 Chorus A | 0–253 | Stereo | Eight voices, four per side, random delay variation; Diffusion acts on voice pairs 3 and 4 |

One retailer page quotes predelay as 0–250 ms for mono and 0–125 ms for stereo programs, which contradicts the table above (Room A is stereo and reaches 255 ms). The UA figures come from v4.4 firmware, so the retailer figure may describe a different revision; I use UA's.

**Not read.** The factory default values per program, the slider and pot laws, the number of user registers, and the 224X/XL control sets (the LARC has a 24-character display and dual 16-position headroom meters from −24 to +12 dBm per [Vintage Digital](https://www.vintagedigital.com.au/lexicon-224x-digital-reverberator/)).

**Plug-in-only additions (not faithful behaviour):** Dry/Wet and Solo; System Noise defeat; Input Gain ±12 dB and Output Level −∞ to +12 dB with Link; Bug Fix switch (the UA logo); Display Hold set to infinite; Power bypass; preset manager. Arturia's Rev LX-24 adds Offset, an advanced panel, ducking, gate and tremolo ([KVR](https://www.kvraudio.com/news-print.php?id=57525)).

## 4. Front-panel layout and signal flow

The remote head puts the display on top, nine program selections in a row, six sliders in the middle and option buttons below, and the audio runs through two converter stages around a 16-bit core.

&#91;embedded content: v4.4 remote head layout · display, 8 program buttons, 6 sliders, option buttons (layout approx.)\]

The sketch follows Universal Audio's description of the controls on its port (display, upper row of program buttons, groups of faders and buttons below, Diffusion buttons beneath the faders), so exact button positions on the real remote are approx. and unverified. The 224X remote head differs, and the LARC of the 224XL has a 24-character display and tape interface.

The original 1979–80 remote head is drawn in 11.5; the signal-flow diagram below stays valid, with the converter and filter details in 11.2 and 11.3.

&#91;embedded content: signal flow · analog in, 12-bit conversion, 16-bit core, analog out, control path\]

Read left to right along each row; the two long elbows are the carriage returns. Only the 8080 control computer and the card names are sourced (Benden); the all-pass structure in the tank is inferred (Section 8). The dry signal never passes through the 224, and outputs A and C are the stereo pair.

## 5. Hardware blocks and components

The 224 is a stack of Multibus I cards, and the card list (from one repair engineer) is the only component-level source I found; no schematic, so every analog value is unknown or inferred.

Main source: [Benden Sound Technology](http://www.lenham.clara.net/sound/lexicon224p1.html), who serviced several units and has "extensive documentation and EPROM images" but did not publish values. Confidence terms follow the legend in the opening section.

| Block (card) | What is known | Confidence |
| --- | --- | --- |
| Chassis and power | 4U welded steel frame, no removable panels; large multi-output linear supply with hard-working electrolytics; front power switch Schadow N30X-2U | Sourced |
| Input stage (AIN) | Two input transformers mounted on the card; input buffer and gain trim; anti-alias filters on RC4558 dual op-amps; 7-pole elliptic with FDNR sections, nominal corner 8 kHz, three trimmed nulls (service manual, see 11.2) | Sourced; corner approx. 8 kHz for the 224 (inferred from the stated 8 kHz bandwidth) |
| Gain ranging | A scaling circuit shifts input gain to give 24 dB extra headroom, so 12-bit conversion covers a 16-bit range at 12-bit precision; service manual: four ranges of 0, 6, 12 and 18 dB, thresholds 5.0 / 2.24 / 1.12 / 0.56 / 0.28 V (see 11.2) | Sourced; the 24 dB figure is superseded by 18 dB total |
| A/D converter | 12-bit successive approximation built around a DAC80 and a comparator, with sample-and-hold multiplexing the channels; the AIN DAC80 is a current-output part, different from the AOUT one | Sourced |
| Floating-point converter (FPC) | Converts the 12-bit floating-point converter data to and from 16-bit fixed point for the DSP; 12-bit mantissa plus 2-bit exponent per the service manual; output rounding unknown (see 11.3) | Sourced |
| ARU (arithmetic unit; Benden calls it ALU) | 16-bit fixed-point multiplier/accumulator from 74-series logic; 100 operations per sample at 20 kHz, i.e. a 2 MHz instruction rate (computed: 100 × 20,000) | Sourced; multiplier width unknown |
| Data memory (DMEM) | 16K words × 16 bit on 4116 DRAM (three supply rails, "temperamental"); 224X has 32K words. 16,384 words at 20 kHz is 0.82 s of total delay (computed) | Sourced |
| Timing and control (T&C) | 6810 static RAM holds the DSP program; small-scale logic decodes it; the 224X has different cards. Program length of 100 steps for the 224 comes from a forum post | Sourced; step count unverified |
| Control computer (SBC) | Intel or National Semiconductor board with an 8080, parallel and serial ports, small RAM and four 2716 EPROMs; runs the UI and loads the DSP with the chosen algorithm | Sourced |
| Non-volatile store (NVS) | Battery-backed static RAM (3 AAA NiCd), more EPROM, from firmware 4.4 on | Sourced |
| Output stage (AOUT) | Voltage-output DAC80; sample-and-hold demultiplexing; RC4558 reconstruction filters; "hefty" drivers into four output transformers on the rear panel | Sourced |
| Remote | Parallel link on 224 and 224X; serial link and a new header board for LARC (224XL); display, five-segment meters, sliders, buttons | Sourced |
| Delay modulation | Mode Enhancement is defined as the time between delay-line updates and the size of each update step, so the tap address is probably stepped by the control computer rather than interpolated; UA says its delay-modulation zipper noise can be reduced but not removed | Inferred |
| Clock and jitter | Source, stability and jitter not documented | Unknown |
| Transformer type, ratio, core | Not documented. Distortion is part of the character per UA's model and Benden's hypothesis | Unknown |

**Published specs (retailer copy of the 224X sheet, not checked against a manual).** Dynamic range 84 dB typical, 81 dB minimum (20 Hz–20 kHz, reverberant mode, reverb times 0–10 s); THD+N 0.05% typical, 0.07% maximum at reference level; frequency response 20 Hz–15 kHz ±1.5 dB and 20 Hz–12 kHz ±0.5 dB ([Vintage Digital](https://www.vintagedigital.com.au/lexicon-224x-digital-reverberator/)). A different retailer lists the original 224 at 12-bit quantisation, THD 0.05% ([Gearspace](https://gearspace.com/gear/lexicon/224), snippet).

**What to carry over from this project's other compendiums.** The confidence legend, the "what to measure" list and the cheapest-first test matrix carry over unchanged. The MXR compendium's linear-filter discretisation (§7.3) applies to the anti-alias, reconstruction and transformer filters. Its "unit variation seed" idea (§7.6) applies to per-unit converter and transformer differences. The TB-303 overview calls for integer control codes with quantisation (the 6-bit pitch DAC), which is the right model for Depth 0–71 and Diffusion 0–63. The clipping solvers, diode models and slew limits do not apply.

## 6. Quirks, per-unit variation, calibration and faults

The behaviours worth modelling are overflow, self-oscillation, converter noise and stepping, and two firmware bugs; hardware faults are repair data, not sound.

**Faithful quirks.**

| Quirk | Evidence | Emulation action |
| --- | --- | --- |
| Arithmetic overflow at loud input, long decay or self-oscillation gives artifacts and an Overflow LED | [UA manual](https://help.uaudio.com/hc/en-us/articles/4419497193492-Lexicon-224-Digital-Reverb-Manual), "fully modeled" | Use 16-bit fixed-point arithmetic with the same overflow rule; the rule itself (wrap or saturate) is not documented, so measure it |
| Extreme settings self-oscillate | UA manual: identical to the original hardware | Do not clamp Bass, Mid or Treble Decay to a stable region |
| Converter noise and quantisation at A/D, D/A and inside the algorithm; parameter zipper noise; transformer distortion; quiescent noise | UA's "System Noise" list, [Sound On Sound](https://www.soundonsound.com/reviews/ua-lexicon-224) | Model as separate switchable blocks (Section 8) |
| Delay-modulation stepping (Mode Enhancement) cannot be fully removed | UA manual | Update modulated taps in discrete steps, with interval and size taken from the two 1–16 controls |
| Toggling Mode Enh resets the algorithm | UA manual | Clear all delay memory on toggle |
| Hall B (P1, P3) gives wrong Bass decay times in some settings | UA manual: original code bug, fixed by UA with a switch | Faithful mode reproduces it; the bug itself is not described, so it must be measured |
| Chorus (P9) can "pop" or "thump" in the right channel for some inputs and settings | Same | As above |
| Original sound is described as dark and grainy next to the later PCM-70 | A forum listener, [Gearspace](https://gearspace.com/board/electronic-music-instruments-and-electronic-music-production/950548-lexicon-224-alternatives.html) | Subjective; use as a sanity check only |

**Per-unit variation.** Nothing is measured or published that I found. Plausible sources, all inferred: ADC/DAC offset and linearity (Benden recommends recalibrating them), transformer differences, ageing capacitors, and firmware version (4.3, 4.4, 8.1, 8.2.1). Expose a single "unit seed" that perturbs converter offset and gain-step error, not the algorithm.

**Calibration.** Benden recalibrates the ADC and DAC "to minimise distortion" but does not publish the procedure; I could not find the trim points.

The service manual's calibration procedures (filter nulls, ADC offset, PLL trim, factory test limits) are in 11.4 and tell you which parts vary per unit: the six filter trims per channel, the ADC and DAC offset pots and the PLL capacitor.

**Known faults** (from [Benden](http://www.lenham.clara.net/sound/lexicon224p2.html), three units from a service centre):

- Power supply: shorted diodes, dried electrolytics; recap the linear supply first.
- Contacts: dirty card-edge connectors and IC sockets cause many faults; an intermittent internal ribbon cable.
- Memory: partly erased EPROMs; NiCd backup cells on the NVS card leak onto the board; 4116 DRAMs need three rails and are temperamental.
- Passives and ICs: many failed analog and digital ICs; tantalum capacitors can short the supply.
- Mechanical: front-panel weight hangs on the power button shaft, so the switch often breaks; welded chassis makes servicing hard; liquid spills.
- Self-test: Lexicon's firmware has comprehensive self-diagnostics that point to the failing card.

None of these should be emulated; they matter only if someone wants a "faulty unit" preset.

## 7. Existing emulations

Universal Audio's plug-in is the only emulation built from Lexicon's original code, so it is the reference to listen against; I found no published measurement comparing any emulation with a real unit, and no hardware or FPGA clone.

| Emulation | Type | What it models | Quality evidence | Gaps and caveats |
| --- | --- | --- | --- | --- |
| [UAD Lexicon 224](https://help.uaudio.com/hc/en-us/articles/4419497193492-Lexicon-224-Digital-Reverb-Manual) (UAD-2 2011, later native) | Software, licensed | Original v4.4 algorithms and control-processor code; input transformers; 12-bit gain-stepping A/D and D/A; overflow behaviour; original bugs switchable | Vendor claim of exactness; [Sound On Sound](https://www.soundonsound.com/reviews/ua-lexicon-224) found it sounds as expected; [CDM](https://cdm.link/arturia-lx-24-recreates-1978-lexicon-224-reverb-but-dont-miss-the-2023-flipside/) calls it the accurate one. Best available, not independently measured | Two outputs at a time of the four; no user registers; adds Dry/Wet, System Noise defeat, bug fixes |
| [Arturia Rev LX-24](https://www.kvraudio.com/news-print.php?id=57525) (2023, $99) | Software | Eight reverb algorithms billed as "perfectly emulated"; 12-bit, 24-bit and Modern converter modes; Mode Enhancement; Crossover and Offset | Press copy says "emulated"; [Production Expert](https://www.production-expert.com/production-expert-1/7-classic-reverb-emulation-plugins-to-check-out-in-2024) says "inspired by". Treat as 224-style, not proven faithful | No chorus program listed; adds ducking, gate, tremolo and a visualiser |
| [Native Instruments RC 24](https://www.musicradar.com/reviews/tech/native-instruments-reverb-classics-573238) (coded by Softube) | Software | Room, Small Hall and Large Hall only | MusicRadar says UAD "comes closer"; a KVR poster found RC 24's plate output unlike UAD's because it has no plate algorithm | Inspired by the 224, not a clone |
| [ValhallaVintageVerb](https://www.musicradar.com/tuition/tech/how-to-recreate-classic-lexicon-and-eventide-effects-in-your-daw-622337) | Software, not a 224 clone | 1970s colour mode filters and downsamples to mimic era bandwidth; 1980s mode is brighter and Lexicon-flavoured | Often cited as the close, cheap alternative | No split-band decay |
| Lexicon PCM Native Reverb Bundle | Software, Lexicon's own | Later Lexicon algorithms | Per MusicRadar, not the 224 | Different algorithms |
| Hardware or FPGA 224 | None found | n/a | Benden calls the 224 "a prime target" for FPGA emulation but published nothing; a 2014 forum post mentions a coming boxed model, unverified | My search was not exhaustive |

**What the existing emulations teach.** Everything the UA port models beyond the algorithm (transformers, gain-stepped converters, zipper noise, overflow) is listed by UA as separate, switchable effects, so a faithful emulation can be built from independent blocks. The Arturia and NI products show the market also wants cleaner, wider options, which belong in a separate added-features layer.

**Correction.** Dattorro-style "plate" implementations found in many open-source reverbs are *not* 224 emulations. The paper says only that its topology is "in the style of Griesinger" ([Dattorro](https://ccrma.stanford.edu/~dattorro/EffectDesignPart1.pdf)), and its own Table 1 uses a 29,761 Hz sample rate, not the 224's 20 kHz.

## 8. Emulation architecture and algorithm options

Build the emulation in three layers, with a 20 kHz, 16-bit fixed-point reverb core at the centre, a switchable "system" layer for the analog and converter behaviour around it, and a separate added-features layer; the core's exact topology is not public, so it is a design proposal built from Dattorro's published plate-class network plus the 224 differences reported on forums.

Costs below are relative (approx.) and assume a modern host at 48 kHz. "Accuracy" means likelihood of matching the real 224 once parameters are measured.

**Layer map**

| Layer | Contents | Faithful? |
| --- | --- | --- |
| 1. Core | Predelay, input diffusion, split-band decay loop, modulated taps, output taps, chorus program, 16-bit arithmetic with overflow, integer control codes | Faithful (to the extent documented) |
| 2. System | Input transformer, anti-alias filter, gain-ranged 12-bit A/D, 12-bit D/A, reconstruction filter, output transformer, quiescent noise, parameter zipper | Faithful, each switchable |
| 3. Added features | Dry/Wet and Solo, Input and Output gain, Link, bug-fix switch, bypass, host-rate clean mode, "Modern" converter, presets | Not faithful; keep out of the core |

**Constants to hard-wire.** Core rate 20 kHz (224) with 16,384 words of delay memory; 224X rate 34.5 kHz (approx.) with 32,768 words; 100 program steps per sample on the 224; 12-bit converters with 24 dB of gain ranging; nine programs, seven algorithms.

**8.1 Sample-rate interface**

| Option | Accuracy | CPU | Flexibility |
| --- | --- | --- | --- |
| A. Polyphase rational resampler with a Kaiser-windowed sinc prototype, e.g. 5/12 at 48 kHz and 200/441 at 44.1 kHz, to a fixed 20 kHz core | High; bandwidth and imaging match the original once the front-end filter is added | Medium | Medium; core rate fixed |
| B. Run the core at the host rate with every delay length scaled by host rate ÷ 20 kHz | Low; no 8 kHz limit, different aliasing, different modulation step timing | Low to high with host rate | High |
| C. Farrow-structure cubic Lagrange resampler to 20 kHz | Medium; passband droop and imaging errors | Low | High; rate can be varied, e.g. for 224X |

Recommendation: A for the faithful path, B only in the added "clean" mode.

**8.2 Anti-alias, reconstruction and transformer filtering (linear part)**

| Option | Accuracy | CPU | Flexibility |
| --- | --- | --- | --- |
| A. Cascade of biquads fitted to a measured magnitude response, discretised by the bilinear transform with frequency pre-warping | High if measured; order unknown now | Low | Medium |
| B. Linear-phase FIR designed by the Kaiser window method | Magnitude good, phase wrong, adds latency | Medium | High |
| C. Fixed Butterworth low-pass near 8 kHz, order 4 | Low; stand-in until measured | Low | Low |

Recommendation: C as a placeholder, then A once a response is measured (Section 9).

**8.3 Transformers (nonlinear part)**

| Option | Accuracy | CPU | Flexibility |
| --- | --- | --- | --- |
| A. Linear filters only | Low; no level-dependent distortion | Lowest | Low |
| B. Linear filters plus a memoryless soft-saturation waveshaper (hyperbolic-tangent curve), 2× oversampled | Medium; no hysteresis or frequency-dependent distortion | Low | Medium |
| C. Jiles–Atherton hysteresis core model with leakage and winding-resistance network | Potentially high; every parameter unknown | High | High |

Recommendation: B, with C reserved until transformer data exist. All parameters here are unknown.

**8.4 Converters**

| Option | Accuracy | CPU | Flexibility |
| --- | --- | --- | --- |
| A. Fixed 12-bit uniform quantiser, truncation | Low; ignores gain ranging | Lowest | Low |
| B. Gain-ranged (block floating-point) quantiser: pick one of several gain ranges from the input level, quantise to 12 bits, restore, so noise rises with level; then convert to 16-bit fixed point | High for the described architecture; range thresholds and step size unknown | Low | Medium |
| C. Option B plus a successive-approximation model with random per-bit weight errors (static INL and DNL), set by the unit seed | Highest; needs measured error | Low | High |

Recommendation: B, with the number of ranges and thresholds as measured parameters; C as the unit-variation option.

**8.5 Reverb core topology**

| Option | Accuracy | CPU | Flexibility |
| --- | --- | --- | --- |
| A. All-pass ring (plate-class) network: predelay, input bandwidth one-pole filter, series input all-pass diffusers, a recirculating tank of all-pass sections with delay lines, decay gain and damping filters, many weighted output taps. Start from Dattorro's published topology and add the reported 224 differences (one extra diffusion and delay stage per tank leg, two input diffuser pairs each feeding one leg, damping filters placed differently, more output taps including some in the predelay) | Medium to high for structure; every delay length and coefficient must be measured | Low | High |
| B. Feedback delay network with a Householder or Hadamard feedback matrix, per-line absorptive filters and modulated lines | Medium; natural split-band decay, but structurally not the 224 | Medium | Very high |
| C. Partitioned convolution (uniform FFT partitions) of measured impulse responses | High for one static setting; cannot follow modulation, Decay Optimisation or input-level behaviour | High | Very low |

Recommendation: A. The all-pass-ring structure of the 224 is inferred, not confirmed, from [Dattorro](https://ccrma.stanford.edu/~dattorro/EffectDesignPart1.pdf) and forum reports on [Freeverb3](https://freeverb3-vst.sourceforge.io/tips/reverb.shtml).

**8.6 Split-band decay (Bass, Mid, Crossover, Treble Decay)**

| Option | Accuracy | CPU | Flexibility |
| --- | --- | --- | --- |
| A. Two-band split of the loop signal with a fourth-order Linkwitz–Riley crossover; each band gets its own loop gain from Jot's rule, gain = 10 to the power of (−3 × delay in samples ÷ (rate × reverb time)); Treble Decay is a separate steep low-pass in the loop | High for the control semantics; crossover shape unknown | Medium | High |
| B. Per-loop first-order low-shelving absorptive filter, with DC and high-frequency gains set from the Bass and Mid times by Jot's rule, plus a one-pole damping low-pass for Treble Decay | Medium; gentler crossover than the real control may have | Low | Medium |
| C. Two first-order sections, a low-pass and its complement, as in Dattorro's damping filter | Low to medium | Lowest | Low |

Recommendation: A for the faithful core. The displayed decay number in the original is an approximation, so do not calibrate to it; calibrate to measured RT60 per band.

**8.7 Delay modulation (Mode Enhancement)**

| Option | Accuracy | CPU | Flexibility |
| --- | --- | --- | --- |
| A. Stepped tap-address update every N samples by a step of k samples, with N from the Amount control and k from Pitch Shift | Highest to the documented controls; reproduces zipper noise | Lowest | Low |
| B. Linear-interpolated delay with a slow sine low-frequency oscillator | Medium; adds time-varying low-pass damping per Dattorro | Low | High |
| C. All-pass interpolated fractional delay with a quadrature-pair oscillator | Smooth; not the original's behaviour | Low | High |

Recommendation: A for faithful mode, C for the clean mode. Stepping interval and step size in samples are unknown; take them from measurement.

**8.8 Diffusion and Decay Optimisation**

| Option | Accuracy | CPU | Flexibility |
| --- | --- | --- | --- |
| A. Lookup table from the 0–63 Diffusion code to all-pass coefficients, fitted to measured echo-density growth | High once measured; must reproduce the peak near 32–37 and the drop above 40 | Lowest | Medium |
| B. Smooth parametric curve with a peak near 32–37 (approx.) | Low to medium | Lowest | High |
| C. Decay Optimisation as an input-level envelope follower that scales diffusion and colour coefficients, versus a switched coefficient set by level threshold | Unknown; attack, release and thresholds are undocumented | Low | Medium |

Recommendation: A for Diffusion; for Decay Optimisation, start with the envelope follower and mark every constant unknown.

**8.9 Arithmetic**

| Option | Accuracy | CPU | Flexibility |
| --- | --- | --- | --- |
| A. 16-bit integer words in delay memory, wider accumulator, a defined overflow rule (wrap or saturate, measure which) | Highest; reproduces overflow and truncation noise | Low | Low |
| B. 32-bit float with explicit rounding to 16-bit grids at each memory write | High; cheaper to implement | Low | Medium |
| C. Double-precision float, no quantisation | Low faithfulness; clean | Low | Highest |

Recommendation: A or B; C for the clean mode. Dattorro reports that magnitude truncation (rounding toward zero) lowers the post-signal noise floor of all-pass lattices by 12–24 dB; whether the 224 used it is unknown, so make rounding mode a measured parameter.

**8.10 Chorus program (P9)**

| Option | Accuracy | CPU | Flexibility |
| --- | --- | --- | --- |
| A. Eight delay voices with delay times following independent random walks, band-limited by a one-pole filter | Matches "varies randomly and independently" in [Sound On Sound](https://www.soundonsound.com/reviews/ua-lexicon-224) and the UA manual; ranges unknown | Low | Medium |
| B. Eight voices with sine low-frequency oscillators at unrelated rates | Medium; audible periodicity | Low | High |
| C. Option A with stepped updates as in 8.7A | Closest to the likely hardware | Lowest | Low |

Voices 3 and 4 per side pass through all-pass diffusion controlled by the Diffusion value; the first two pairs have overlapping delay ranges (UA manual).

**Faithful versus added, summary.** Keep the integer control codes, the stepped modulation, the overflow rule and the original bugs in the faithful path. Put smooth parameter ramps, all-pass interpolation, double precision, a Modern converter and any tonal EQ in the added-features layer, defaulted off.

## 9. Corrections, open questions and validation matrix

The biggest gaps are the per-algorithm delay lengths and coefficients, the converter gain-ranging law and the overflow rule; each needs a measurement from a real unit, not more reading.

**Corrections to claims in other sources**

| Claim (source) | Status | Position taken here |
| --- | --- | --- |
| "16-bit A/D and D/A at 48 kHz", $8,800 launch price, 224X (1982) "added modulation effects", 224XL (1985) "optimized presets" ([Vintage Technology Archive](https://vintagetechnologyarchive.com/synth/lexicon/224/), search snippet only) | **Very likely wrong** | Converters are 12-bit with 24 dB gain ranging at 20 kHz (224) ([Benden](http://www.lenham.clara.net/sound/lexicon224p1.html), [Dattorro](https://ccrma.stanford.edu/~dattorro/EffectDesignPart1.pdf)); list price was $7,500–7,900 ([Mix](https://www.mixonline.com/technology/1978-lexicon-224-digital-reverb-383667)); Mode Enhancement already exists in the v4.4 firmware that UA modelled; the XL's change was the LARC remote, in 1984 |
| "3U rack mainframe" for the 224XL ([Vintage Digital](https://www.vintagedigital.com.au/lexicon-224xl/), snippet) | Conflicts | Mix, Benden and UA all say four rack spaces (4U) |
| "First digital reverb to use a microprocessor" (Vintage Digital brand page, snippet) | Disputed | The EMT 250 reached the US first, and Mix says Griesinger added a microcomputer *after* seeing the 250; I make no priority claim |
| Dattorro's plate network is the 224 algorithm (common belief in plug-in forums) | **Myth** | The paper says "in the style of Griesinger" and uses 29,761 Hz; the 224 ran at 20 kHz. It is a close relative, not the 224 code |
| 224X "doubled" the sample rate | Imprecise | Benden says bandwidth doubled to about 16 kHz; the 34.5 kHz figure is 1.7× 20 kHz, and is second-hand |
| 224 introduced in 1978 versus 1979 | Both sourced | Unveiled 1978 at AES; Dattorro's 1979 is probably first shipping (approx.) |
| Arturia Rev LX-24 is a "perfect" emulation | Marketing claim | Another outlet calls it "inspired by"; unproven |

**Open questions** (in the order they block the emulation)

1. Delay lengths, all-pass coefficients and tap weights for each of the seven algorithms (needs the ROM images or impulse-response measurements).
2. Slider and control laws: how Bass, Mid, Crossover, Treble Decay, Depth and Predelay map to codes, and how the displayed decay time is computed.
3. Converter linearity and sample-and-hold droop (gain-ranging steps and thresholds are now sourced, see 11.2).
4. 224X filter order and corner (the 224 is 7-pole elliptic with an 8 kHz corner, sourced in 11.2).
5. Overflow rule (saturation likely from the ARU SAT line, unproven) and rounding mode (truncate, round, or magnitude truncation).
6. Mode Enhancement step interval and size in samples; Decay Optimisation envelope constants.
7. The exact Hall B bass-decay bug and the Chorus right-channel pop.
8. Factory default values per program; user-register count.
9. Transformer characteristics (core, level-dependent distortion, bandwidth).
10. Exact 224X sample rate and how its algorithms differ from v4.4; firmware 8.x behaviour.
11. Further serial-number or board-revision boundaries (one found: serial 2102 and the software v2 ECO, see 11.4).
12. Measurement access: check the licence terms before analysing any commercial plug-in's output; a real unit is the clean source.

**Validation matrix.** Ordered cheapest and most diagnostic first; a wrong low-level block invalidates the tests above it. Targets marked *computed* come from the arithmetic here, *spec* from the retailer copy of the 224X sheet, *measure* means a value from a real unit.

| Level | Test | What to check | Target |
| --- | --- | --- | --- |
| 1. Constants | Arithmetic on core settings | Rate, memory, program length | 20 kHz; 16,384 words = 0.82 s of delay; 100 steps per sample = 2 MHz instruction rate (*computed*, sourced inputs) |
| 2. Front-end response | Small-signal sweep through System layer only | Bandwidth | About 8 kHz for the 224 (approx.); 224X spec 20 Hz–15 kHz ±1.5 dB and 20 Hz–12 kHz ±0.5 dB (*spec*); fit biquads to *measure* |
| 3. Converter noise | Silence and low-level sine through the converters, reverb off | Noise versus level | An ideal fixed 12-bit path gives about 74 dB signal-to-noise (*computed*: 6.02 × 12 + 1.76); the spec of 84 dB typical, 81 dB minimum is consistent with gain ranging (*inferred*); THD+N 0.05% typical (*spec*) |
| 4. Control mapping | Sweep each slider and code | Monotonicity, ranges | Bass and Mid 0.6–70 s; Crossover and Treble Decay 100 Hz–10.9 kHz; Depth 0–71; Diffusion 0–63; Mode Enh 1–16 |
| 5. Decay time | Backward-integrated energy decay of a decay recording, per band | Measured RT60 versus setting | Match *measure*, not the display, because the display is approximate |
| 6. Split-band | Bass ≠ Mid at several Crossover values | Band-specific RT60 and crossover shape | Crossover has no effect when Bass = Mid; matches *measure* otherwise |
| 7. Diffusion | Normalised echo-density profile versus Diffusion code | Build rate | Fastest near 32–37; denser-sounding less above 40; Acoustic Chamber fixed |
| 8. Modulation | Sine input, spectrum of tail | Sidebands, stepping | Lower Amount and higher Pitch Shift give stronger movement; zipper not fully removable |
| 9. Overflow and oscillation | Rising level; extreme decay settings | Overflow LED, wrap or saturate, self-oscillation | Matches *measure*; no clamp on decay |
| 10. Per-program impulse response | One impulse per program at defaults | Energy-decay curve, echo density, stereo correlation, output assignment (A–D) | Within tolerance of *measure*; P4 mono in, P8 mono-summed in, P9 chorus voices |
| 11. Bugs and parameter zipper | Hall B bass decay; Chorus right-channel pops; slider sweeps | Bug-for-bug and zipper | Present in faithful mode, absent when bug-fix is on |
| 12. A/B in context | Drums into Percussion Plate A, vocal into Vocal Plate, hall on strings | Listening | Faithful path first, then added layer; if a number fails here, re-tune against a real unit rather than trust any written source, this document included |

## 10. Sources

Six web pages were fetched and read in full; everything else was seen only as search-result snippets and is marked so in the text. No owner's manual, service manual, schematic, patent or firmware image was read.

**Opened and read (fetched)**

- [Benden Sound Technology, Lexicon 224/224X restoration, page 1](http://www.lenham.clara.net/sound/lexicon224p1.html): card-by-card hardware, revisions, firmware numbers.
- [Benden, page 2](http://www.lenham.clara.net/sound/lexicon224p2.html): faults, refurbishment, listening notes.
- [Dattorro, "Effect Design Part 1", JAES 1997](https://ccrma.stanford.edu/~dattorro/EffectDesignPart1.pdf): plate-class topology, 224 and EMT 250 memory and rate footnotes, magnitude truncation, delay modulation.
- [Universal Audio, Lexicon 224 Digital Reverb Manual](https://help.uaudio.com/hc/en-us/articles/4419497193492-Lexicon-224-Digital-Reverb-Manual): controls, ranges, programs, bugs, system-noise list.
- [Sound On Sound, UA Lexicon 224 review (Oct 2011)](https://www.soundonsound.com/reviews/ua-lexicon-224): program descriptions, price, behaviour.
- [Freeverb3, Reverb Algorithms Tips](https://freeverb3-vst.sourceforge.io/tips/reverb.shtml): forum-sourced notes on Lexicon structures (hearsay, flagged where used).

**Search snippets only (not opened in full)**

- [Mix, 1978 Lexicon 224](https://www.mixonline.com/technology/1978-lexicon-224-digital-reverb-383667): origin story, launch prices.
- [Vintage Digital: 224](https://www.vintagedigital.com.au/lexicon-224-digital-reverberator/), [224X](https://www.vintagedigital.com.au/lexicon-224x-digital-reverberator/), [224XL](https://www.vintagedigital.com.au/lexicon-224xl/): specs, dates.
- [Little Devil Studios](https://littledevilstudios.com.au/2015/02/lexicon-224-digital-reverb/) and [Gearspace 224XL thread](https://gearspace.com/threads/lexicon-224xl.992645/): revision dates and converter chips (unverified).
- [Wikipedia, Lexicon (company)](<https://en.wikipedia.org/wiki/Lexicon_(company)>); [David Griesinger's home page](http://www.davidgriesinger.com/acoustics_today/).
- [Sound On Sound, Classic Tracks: Sledgehammer](https://www.soundonsound.com/techniques/classic-tracks-peter-gabriel-sledgehammer); [Tape Op review of the UA plug-in](https://tapeop.com/reviews/gear/85/lexicon-224-digital-reverb-plug-in); [UA blog](https://www.uaudio.com/blogs/ua/everything-you-need-to-know-about-reverb); [Danny Chesnut's UA page](https://www.dannychesnut.com/Recording/Lexicon/224/index.html).
- Emulations: [KVR on Arturia Rev LX-24](https://www.kvraudio.com/news-print.php?id=57525), [MusicRadar on Rev LX-24](https://musicradar.com/reviews/arturia-rev-lx-24-review), [CDM](https://cdm.link/arturia-lx-24-recreates-1978-lexicon-224-reverb-but-dont-miss-the-2023-flipside/), [MusicRadar on NI Reverb Classics](https://www.musicradar.com/reviews/tech/native-instruments-reverb-classics-573238), [Production Expert](https://www.production-expert.com/production-expert-1/7-classic-reverb-emulation-plugins-to-check-out-in-2024), [MusicRadar on Lexicon and Eventide emulation](https://www.musicradar.com/tuition/tech/how-to-recreate-classic-lexicon-and-eventide-effects-in-your-daw-622337), [Synthtopia](https://www.synthtopia.com/content/2023/04/06/arturia-rev-lx-24-copies-classic-blade-runner-reverb/), [Gearspace 224 alternatives](https://gearspace.com/board/electronic-music-instruments-and-electronic-music-production/950548-lexicon-224-alternatives.html).
- [Vintage Technology Archive, Lexicon 224](https://vintagetechnologyarchive.com/synth/lexicon/224/): cited only to correct it (Section 9).

**Service manual (read in full, as page images).** Lexicon 224 Service Manual, 113 PDF pages, dated 1 July 1980 (diagnostics 5 June 1980; board documents 010-01600, 010-01601, 010-01603, 010-01604, 010-01857, 010-01858; schematics 060-01319 to 060-01324, 060-01359, 060-01360). Supplied by the user, a MusicParts.com copy; no public link. Not covered: the response plots in its Fig. 1A/1B and any operating-guide pages.

**Project files read:** the compendium overview and research queue in full; the BD-2 compendium's first 130 lines; heading outlines and the test-matrix and measurement sections of the MXR compendium. The TB-303 compendium is not among the project files supplied here, so it is known only through the overview; the Magus Pro file was not read beyond its headings.

## 11. Addendum: 1980 service manual (primary source)

The Lexicon 224 Service Manual (dated 1 July 1980; diagnostics section 5 June 1980) settles most hardware gaps in sections 5, 6 and 9: converter format, a 7-pole elliptic filter on both sides, gain-ranging thresholds, power supplies, the original panel and the factory test limits. Where this addendum and an earlier section disagree, this addendum wins. "SM" below means that manual; page numbers are PDF pages (approx.). It is a scanned manual, read page by page as images; the filter response plots (its Fig. 1A/1B) are not in the copy I received.

### 11.1 Corrections to earlier sections

| Topic | Earlier text | Service manual says | Confidence |
| --- | --- | --- | --- |
| Gain ranging | "24 dB extra headroom" (Benden), steps unknown | Four ranges, 0 / 6 / 12 / 18 dB: 18 dB total in 6 dB steps, chosen per sample immediately before each conversion (the manual calls it "instantaneous floating point"). Treat the 24 dB figure as loose wording | Sourced |
| Converter format | 12-bit floating point, exponent width unknown | 12-bit mantissa plus 2-bit exponent; the FPC shifts it left 0–3 places into a 16-bit fixed-point word. The overview calls that word two's complement, the FPC section calls it offset binary, so the sign convention is unresolved | Sourced; sign convention unresolved |
| Filter order | "order and corner not given" | 7-pole elliptic (Cauer) filters synthesised from FDNR sections on 4558 op-amps, on both inputs and all four outputs; nominal corner 8 kHz | Sourced |
| Block name | "ALU" | ARU (arithmetic unit): multiplier-accumulator with input and result registers | Sourced |
| Sample period | 20 kHz, 100 steps, 2 MHz instruction rate (computed) | 500 ns microinstruction cycle; the FPC debug counter wraps at 100 states = 50 µs, so 20 kHz is now sourced for normal running. The clock comes from a phase-locked loop on the T&C card; its reference is not stated | Sourced; PLL reference unknown |
| T&C program memory | 6810 SRAM, length from a forum | Program memory is 128 words × 32 bits. Four 6810 (128 × 8) would fit; chip count inferred | Sourced; chip count inferred |
| Output DAC | Voltage-output DAC80 | 12-bit DAC, −5 to +5 V, then a 4-tap precision divider (1, 1/2, 1/4, 1/8, ±0.05 %), so the output side is floating point too | Sourced |
| Control computer | Intel or National 8080 board | 8080A with 8224 clock generator and 8228 system controller (18.432 MHz crystal), National BLC 80/11 or Intel SBC 80/10A; 1 K RAM at 3C00–3FFF, four 2716 EPROMs (8 KB) at 0000–1FFF; two 8255 parallel chips and one 8251 serial chip | Sourced |
| Remote head link | Parallel link | 25-conductor cable; 10 V AC from a separate transformer secondary, rectified and regulated inside the head (7805); 8-bit data port, 4-bit digit address, 3 control lines | Sourced |
| Memory | 4116 DRAM | 16 × 2117-4 (16K × 1), 14-bit address, 16 K words × 16 bit; refresh happens inside microinstruction cycles that do not touch memory | Sourced |
| Original panel | Sketch based on UA's v4.4 port | Original has 8 program buttons, IMMED / SET / CALL / SHIFT, registers A–D, six pot-select buttons and a three-digit display; see 11.3 and the drawing in 11.5 | Sourced |
| Input level | unknown | Nominal full scale is +13 dBm (5 V peak) at the buffer output; spec input range +8 to +18 dBm, trim range 15 dB; most units work from +7 to +22 dBm | Sourced |

### 11.2 Analog chain with component values

Values are read from the scanned schematics (AIN 060-01321 rev 6, AOUT 060-01322, transformer board 060-01359, all 1979) and the theory text. A misread digit is possible, so "sourced" here means "on the schematic as scanned"; computed figures are marked.

**Input card (AIN), per channel**

| Stage | Parts and values | Notes | Confidence |
| --- | --- | --- | --- |
| Input transformer | T1 / T2 on the card; ratio and core not stated | Fed from XLR through the transformer board RFI network (820 Ω per leg, 150 pF, ferrite bead) | Sourced; transformer data unknown |
| Gain trim | 50 k pot (R1 / R2) with 10 k shunt, 2 k series, 1N4148 clamps to ±7 V | Trim range 15 dB; set so a +12 dBm 1 kHz test tone gives ±5 V at the buffer output | Sourced |
| Buffer | Half a 4558 (U1); feedback 3.09 k, ground leg 2.00 k | Gain 2.55 (computed, ≈ +8 dB). Output +13 dBm = 5 V peak = ADC limit | Sourced; gain computed |
| Filter sections | Three FDNR-type sections, each a 4558 half: (7.68 k, 1000 pF), (13.7 k, 1000 pF), (3.09 k, 1000 pF) with 10 k / 10 k / 10 pF and a 5 k trim each; series resistors 23.7 k, 22.6 k, 22.6 k, 10.5 k; 1000 pF shunt; LF356 buffer after (was LM741, then LF351/353) | 7-pole elliptic, nominal corner 8 kHz. Trims null at 11.815, 10.240 and 19.120 kHz | Sourced |
| Pre-emphasis | Half a 4558 with 15.4 k, 33.2 k, 48.7 k and 2400 pF 2.5 % | +2.6 dB at 2 kHz, +8.15 dB at 8 kHz; the output side has the mirror de-emphasis | Sourced |
| Aperture correction | Second 4558 half: 15 k, 15 k, 2200 pF, 240 pF (both 2.5 %) | Compensates the sample-and-hold sinc loss | Sourced |
| Track and hold | LM398 per channel, 1000 pF polypropylene hold capacitor; 4053 switch; one channel tracks while the other holds | CH1L selects which channel is converted | Sourced |
| Gain switch amplifier | LF356 with 1 k × 8 0.1 % resistor network and a CD4051 | Gain 0, 6, 12, 18 dB in 6 dB steps, offset must stay under 80 mV | Sourced |
| Gain-range detector | Precision full-wave rectifiers (4558, 1 % 10 k), four LM339 comparators, thresholds 5.0, 2.24, 1.12, 0.56, 0.28 V (nominal) | Each step is 6 dB; a signal under 45 % of full scale gets 6 dB more gain. The 5 V and 0.28 V comparators only drive the headroom meter | Sourced |
| ADC | DAC-80 (12-bit, current out), Am2509 SAR register (schematic label), LM211 comparator, offset pot 100 k | 13 conversion-clock pulses per sample; zero input must give 100000000000 or 011111111111 | Sourced |

**Output card (AOUT) and transformer board**

| Stage | Parts and values | Notes | Confidence |
| --- | --- | --- | --- |
| DAC | DAC80 voltage-out, −5 to +5 V, offset pot 25 k with 5 k each side to ±15 V | One DAC time-shared by four channels; updated with a 12-bit word | Sourced |
| Gain switch | CD4051, thin-film 1 k × 8 0.1 % network (4 k / 2 k / 1 k / 1 k), LF356 follower | Output gain 1, 1/2, 1/4, 1/8 ±0.05 % | Sourced |
| Sample and hold | 4016 switch, four 750 pF polypropylene capacitors, CA3240 followers with 470 Ω series resistors | Channel strobe goes high 2.93 µs after data is valid, sample window 6.8 µs | Sourced |
| De-emphasis | Half a 4558: 48.7 k, 33.2 k, 15.4 k, 2400 pF 2.5 % | Mirror of the input pre-emphasis | Sourced |
| Reconstruction filter | Three FDNR sections as on the input (23.7 k, 22.6 k, 22.6 k, 10.5 k series; 7.68 k, 13.7 k, 3.09 k with 1000 pF; 5 k trims), then 4558 buffer | 7-pole Cauer; same three null frequencies; four independent copies | Sourced |
| Level and driver | 50 k level pot (+3 to −13 dBm at the pot), LF356 driving MJE180 / MJE170 complementary emitter followers, 33 Ω emitter resistors, 200 Ω series, 0.01 µF | Outputs must deliver +8 to +18 dBm into 600 Ω | Sourced |
| Output transformers | T1–T4 on the transformer board; 620 Ω and 150 pF with ferrite beads per output; voltage gain about 2.5 | Core, bandwidth and distortion unknown | Sourced; transformer data unknown |
| Power-on mute | 2N3904 with 130 k and 22 µF | Audio enables 2–3 s after power-up | Sourced |
| Analog rails | ±7 V from ±15 V through 270 Ω and zeners (1N751 on AIN, 1N754 on AOUT) | Healthy range 6.3–7.7 V | Sourced |

The first null (11.815 kHz) sits where a 20 kHz sample rate folds back onto about 8.2 kHz, just above the passband edge, and 19.12 kHz sits near the sample rate itself; the purpose of each null is inferred, the frequencies are sourced.

### 11.3 Digital core, converters, control computer, original panel

**Digital signal path** (the "high speed processor" is the T&C, DMEM and ARU cards together)

| Block | What the manual says | Emulation meaning | Confidence |
| --- | --- | --- | --- |
| Input conversion | Each channel is converted in its own 25 µs half of the 50 µs sample period (CH1L toggles every 25 µs): one channel tracks while the other holds. 13 conversion-clock pulses per SAR run | The right input is sampled half a sample (25 µs) after the left; a faithful model delays one channel by half a sample | Sourced; the L/R order is inferred |
| Float-to-fixed | Gain code 00, 01, 10, 11 (0, 6, 12, 18 dB) makes the FPC shift the 12-bit result left 4, 3, 2, 1 places into the 16-bit word, zero filled | Effective input word = 12-bit code × 2^(gain step) re-referenced; small signals gain up to 3 extra bits, low bits are zero, not dithered | Sourced |
| Fixed-to-float (output) | 16-bit word is shifted left until the top two bits disagree or three shifts are done; the top 12 bits go to the DAC; the shift count (0–3) selects the 1 / 1/2 / 1/4 / 1/8 divider | Output is 12-bit mantissa with a 2-bit exponent: quantise the 16-bit word to 12 bits after normalising, then scale back. Rounding of the dropped 4 bits is not stated (truncation likely, inferred) | Sourced; rounding inferred |
| Output multiplexing | One DAC serves A, B, C, D in turn; each held on its own capacitor; DAC needs 2.93 µs to settle, sample window 6.8 µs | Outputs are not simultaneous; exact offset between channels is not stated | Sourced; offsets unknown |
| Headroom meter | Peak detect per channel: LEDs at −24, −18, −12, −6, 0 dB; lit LEDs by input level: +12 dBm five, +11 dBm four, +5 dBm three, −1 dBm two, −7 dBm one, −13 dBm none (SM test table) | Meter is driven from the gain-range comparators, so it reads the pre-gain peak in 6 dB bins, not an RMS or true-peak level | Sourced |
| DMEM | 16 × 2117-4 (16 K × 1) = 16,384 words × 16 bit. A 14-bit current-position counter (74LS393 and 74LS283 adders) advances once per sample; every address is current position minus an offset carried in the microinstruction | All delays are integers in samples. CORRECTED by ROM analysis (section 12): the microcode does two-tap linear interpolation (offsets N and N-1, weights summing to 32) and the 8080 rewrites those two coefficients to set the fractional position | Sourced |
| Memory cycle | 500 ns microinstruction; RAS, row select, CAS in sequence; refresh uses cycles that make no memory access | No audible refresh effect expected; faults appear as dropouts or noise (section 6) | Sourced |
| X register | Four octal tristate registers, the only data path between 8080 and the high speed processor; 8 output ports (0–7) and 10 input ports (0–9) decoded on DMEM | The 8080 patches coefficients, delay offsets and modulation through this register, one byte pair at a time | Sourced |
| T&C | 128-word × 32-bit program memory loaded from the 8080; micro-instruction register; PLL clock; calibration of loop voltage 3.6–4.0 V and jitter ≤ 10 ns | Programs (algorithms) are microcode; coefficient values live in the instruction words (inferred) | Sourced; coefficient storage inferred |
| ARU | Multiplier-accumulator with input and result registers; SAT (saturation) line on the backplane, and an OVFL lamp on the panel | Overflow appears to saturate rather than wrap (inferred from the SAT line); accumulator width not stated | Inferred |
| Self-test and FPC debug | With ARU, DMEM and T&C removed the FPC runs alone: input 1 to outputs A and B, input 2 to C and D, 50 µs sample interval from the 8080's 2.048 MHz clock | Useful validation reference: this is a pure converter loop | Sourced |

**Control computer.** 8080A at 2.048 MHz (18.432 MHz crystal through an 8224), 1 K RAM at 3C00–3FFF, four 2716 EPROMs. The four 2716s go in sockets U23, U24, U25, U26; contents depend on the programs purchased. 8251 serial port with jumper-selectable baud 110–9600 (factory setting 300). 8255 "A" port runs bidirectionally to the remote head with port C as handshake and port B giving four digit-address bits.

**Original front panel (1979–80 remote head)**

| Item | Detail | Confidence |
| --- | --- | --- |
| Display | Three seven-segment digits with decimal points; unit LEDs SEC, kHz, POS, dB (label hard to read), OVFL | Sourced |
| Programs | Eight program push buttons with eight LEDs | Sourced |
| Function keys | IMMED, SET, CALL, SHIFT (four LEDs); registers A, B, C, D (four LEDs) | Sourced |
| Sliders | Six 10 k linear-law slide pots read by an ADC0817 (8-bit, 256 codes, ratiometric on 5 V, 70–130 µs per conversion); six pot-select buttons labelled BASS, RMID, CBASS, TREBLE, DEPTH, PRE DELAY with LEDs | Sourced; taper assumed linear from the ratiometric read |
| Headroom | Two meters of five LEDs each (A and B), shared with the pot-select LEDs on digits 6 and 7 of the scan | Sourced |
| Scan | Eight digits time-multiplexed through a 74LS42; switches are normally-open, read in three banks through 1N283 germanium diodes; display refresh is software timed, a one-shot cuts the LED drive if the 8080 hangs | Sourced |
| Cable | 25-way ribbon to the mainframe, 10 V AC power inside it, the head rectifies and regulates locally | Sourced |

The six pot-select labels differ from UA's slider names; RMID is probably the mid-band decay and CBASS the bass/mid crossover (inferred). Slider position is therefore quantised to 8 bits, and the firmware maps it to a code; the mapping itself is still unknown.

### 11.4 Power, calibration, factory limits, diagnostics

**Power supply** (SM, drawing 060-01324)

| Rail | Regulator | Rating and protection | Confidence |
| --- | --- | --- | --- |
| +5 V | µA723 with 2N5885 pass pair, foldback current limit, crowbar SCR with 6.1 V zener | 10 A continuous, 15 A 3AG fuse, voltage and current limit adjustable; short-circuit under 3 A | Sourced |
| −5 V | 7905 | 250 mA, 2.5 A fuse, thermal and current limit | Sourced |
| +12 V | LM317K | 1.25 A, 2 A slow fuse; sequenced so it cannot come up before −5 V (matters for the DRAM bias) | Sourced |
| −12 V | 7912 | 150 mA | Sourced |
| ±15 V | LM317 plus 7912 slaved by an LM301 so −15 V tracks +15 V, with a balance trim | 750 mA, one fused secondary; analog and digital grounds are separate and joined only in the chassis | Sourced |
| Remote head | 10 V AC from its own secondary, fused on the transition board | Rectified and 7805-regulated inside the head | Sourced |
| Mains | Dual-primary transformer, 100/115 V taps, two DPDT selectors | 3.0 A slow at 100/120 V, 1.5 A slow at 220/240 V; fan on one 115 V primary | Sourced |

**Calibration and adjustment procedures** (these tell you which parts vary per unit)

- Input gain pots R1 and R2: set for ±5 V at the buffer output with +12 dBm, 1 kHz; most units then sit near the pot ends of travel.
- Input and output filters: three null trims per channel (11.815, 10.240, 19.120 kHz; the output side uses 10.2396 and 19.1204 kHz in its note), each trimmed with a 0.5 V peak tone; trimmers are then sealed with green lacquer. A filter that cannot be nulled means a wrong component value.
- ADC offset: trim R90 so the MSB dithers equally between 0 and 1 with the gain stage disconnected, which guarantees code 100000000000 or 011111111111 for zero volts and good gain-step matching. GSA offset must be under 80 mV.
- Output DAC offset trim and the four output level pots (−13 to +3 dBm at the pot, set for +12 dBm out).
- T&C: trim C32 so the PLL control voltage reads 3.6–4.0 V (3.8 V preferred); jitter 10 ns or less; verify that the loop re-locks after a forced unlock.
- Standard test level is 1 kHz at +12 dBm; analog rails (±7 V) must read 6.3–7.7 V.

**Factory test limits** (all channels, 600 Ω terminated; use these as targets for any measured or modelled unit)

| Measurement | Limit | Condition |
| --- | --- | --- |
| THD + noise | below 0.05 % | +12 dBm in, 1 kHz |
| THD + noise | below 0.5 % | 0 dBm in, 8 kHz |
| Noise, 20 Hz–20 kHz | below −68 dBm (80 dB below +12 dBm) | inputs terminated |
| Noise, A-weighted | below −80 dBm (92 dB below +12 dBm) | inputs terminated |
| Channel separation | at least 60 dB (below −48 dBm) | left to right and right to left |
| Output level | +8 to +18 dBm into 600 Ω | after setting +12 dBm |
| Input range | spec +8 to +18 dBm; most units accept +7 to +22 dBm | input gain pot near an end of travel |
| Frequency response | 100 Hz–10 kHz sweep within 0.5 dB of the factory plot | plot not in my copy |
| Power-on mute | audio enables 2–3 s after power-up |  |

**Diagnostics and faults added by the manual**

- Error codes E00–E93 are shown as hex patterns on the program and mode LEDs; a self-test mode loops inputs to outputs and checks the converter path (about 95 % / 85 % coverage claimed).
- E31 and E80 are non-fatal on early units.
- DMEM has two LEDs for "MS generator error" and "row select error", timing watchdogs on the memory strobes.
- Swapping a memory chip changes the noise floor by about +6 dB (a bad-chip signature, from the diagnostics section).
- Software version 2 added Decay Optimisation and needs a hardware ECO on the T&C card (pin 1 of the 74LS08 at IC36, a 500 ns processor pause); units before serial 2102 may also need a protect-circuit change. This is the first serial-number boundary found, replacing "none found" in section 1.

### 11.5 Original panel, emulation consequences, what is still unknown

&#91;embedded content: original 1979-80 remote head · controls and labels sourced, proportions approx.\]

This replaces the v4.4 sketch in section 4 as the faithful 1979–80 layout; the v4.4 sketch stays valid for the UA plug-in port. The original has registers A–D and the IMMED, SET, CALL and SHIFT keys; this manual is a service manual, so what each key does is not described in the pages I read. The operating behaviour of those keys remains unverified.

**What the manual changes for the emulator**

- Input path: model the transformer, 7-pole elliptic filter with the three nulls at 11.815, 10.240 and 19.120 kHz, then +2.6 dB at 2 kHz and +8.15 dB at 8 kHz pre-emphasis, then a 12-bit conversion with per-sample 0/6/12/18 dB ranging. The output mirrors it with a 12-bit DAC, 2-bit exponent and de-emphasis.
- Bandwidth: the nominal 8 kHz corner and a 20 kHz sample rate are now both sourced; a flat 20 kHz bandwidth would be wrong for the 224 (the 224X raised both).
- All delays are whole samples at 20 kHz with one shared position counter; fractional delay is done by a 2-tap crossfade whose coefficients the 8080 rewrites (section 12.4); do not add all-pass interpolation.
- Left and right inputs are sampled 25 µs apart; outputs are time-multiplexed from one DAC.
- Noise and distortion targets: the limits in 11.4 are the factory ceiling, so a unit in good order will measure better; use them as bounds, not as typical values.

**Still unknown after the manual**

- Delay lengths, coefficients and tap weights of the seven algorithms (microcode content is in ROM, not in this manual).
- Slider-to-parameter laws and the displayed decay-time calculation; the sliders reach the firmware as 8-bit codes, which bounds how fine the original control was.
- Mode Enhancement step size and Decay Optimisation constants.
- Overflow behaviour (saturation likely, not proven) and the rounding rule.
- Transformer ratio, core and distortion; filter response plots; the PLL reference frequency.
- The 224X converter rate and its algorithm differences (the manual covers the original 224 only).

## 12. ROM analysis addendum (V2.2, V3.2, V4.3, V4.4, 224X V8.1, 224XL V8.2.1)

Reading the supplied ROMs answers most of the open questions in section 9: the program data, coefficient encoding, parameter-to-coefficient laws and fractional-delay method are now known; a few microword control bits and ARU scaling are not.

### 12.1 Record formats

- **224 V4.3/V4.4:** each program is a 512-byte record in the data ROM(s). The loader (0A3C, 0A68-0B87) copies the whole record to program memory at 4000h. Bytes before 0x70 are a header holding flags and a patch script; words 28..127 start at record offset 0x70 as 4-byte microwords \[off\_lo, off\_hi (top 2 bits flags), A, B\].
- **Program memory:** 128 words x 32 bits at 4000h-41FFh. Byte 3 of word n sits at 4003h+4n. The 8080 patches words there to apply slider values.
- **224X/XL:** 682-byte records (2-byte prefix + 170 words), stride 0x2AA, three per 2 KB. 224X V8.1 has 11 records; 224XL V8.2.1 has 12 (key 81 is new). Per the Lexicon memo: faster T&C/ARU, 32K DMEM, 15 kHz bandwidth, FPC unchanged.
- **V2.2/V3.2:** different header grammar, not yet decoded.

### 12.2 Program keys and version history

- Program-select code (routine 029B) to record key: 01 to 01, 02 to 45, 04 to 01, 08 to 84, 10 to 45, 20 to 06, 09 to 0C, 0C to 1C, 21 to 0E. That is nine programs on seven algorithms, consistent with section 3 (P1 = P3, P2 = P5).
- V4.3 to V4.4 did not change the DSP microcode; earlier versions changed programs substantially (see lexicon224\_version\_history.xlsx).
- Delivered tables: lex224\_v44\_microwords.csv, lex224\_v44\_programs.json, lex224X\_v82\_programs.json, lex224\_patch\_scripts\_v43\_v44.csv (352 decoded entries).

### 12.3 Coefficient encoding and patch script

- **Coefficient field** (firmware 0E1D/0E2E): sign is + when byte 2 bit 7 = 1; magnitude = (\~(byte3 >> 2)) & 0x3F; byte 3 bits 1:0 are preserved. Scale: 32 = unity (needs ARU confirmation).
- **Patch script:** the header lists sections A-N. Each entry points at 4003h+4n and carries the ROM original byte (read via 3E2A + address). The 8080 keeps its working table at 3E70. Each section's count is stored with the previous section's table pointer, so labels shift by one in raw dumps.
- **Section roles:** N = modulated 2-tap crossfade. D/E = complementary pair (x, 32-x) from param 3. F/G = complementary pair from param 4. H-K = four gains from interpolated tables (106D/107D/108D) from param 5. C = loop gain from curve table 0F4A, param 1. B = signed difference (mid - bass) from param 2, with L using their average. M = fine predelay crossfade (3F6E). A = per-word values, probably predelay offsets (unverified).

```text
Curve table 0F4A (9 rows x 5):
38 48 58 62 65 | 30 49 57 61 65 | 28 45 55 60 65
22 41 52 59 64 | 45 56 60 62 66 | 24 43 53 59 63
21 41 51 58 63 | 18 39 50 58 63 | 12 33 46 56 63
```

### 12.4 Fractional delay and modulation

- Interpolation is two taps at offsets N and N-1 with weights (w, 32-w) summing to 32. The 8080 rewrites the weights, so the audio path stays a fixed microprogram.
- Modulation source: routine 0C7C-0DD8 steps a pointer over 0000-0FFFh, which is code ROM, so the random modulation is probably the firmware bytes themselves. Probable, not confirmed.

### 12.5 Slider to section to coefficient map

Obtained by running the real V4.4 handlers in an 8080 emulator over slider codes and diffing program memory. Slot numbers are the array positions 3F66-3F6A; which front-panel pot feeds which slot is not yet confirmed.

| Slot | Handler | Effect (word numbers are program words; second half is the mirrored channel, +50) | Confidence |
| --- | --- | --- | --- |
| 1 (3F66) | 0E3D | Loop gain through curve table 0F4A. Key 01: words 40/90, code 16 gives 16/-16, code 48 gives 48/-48, saturates at 63. Key 06: word 38/88. Key 84: word 39/86. Key 45 drives words 40, 43, 46, 52 plus a mirror set | High for which words, medium for law |
| 2 (3F67) | 0DE6 | Signed difference coefficient on one word per channel (key 01: 39/89; 45: 44/94; 84: 37/84; 06: 36/86). At low codes (0-16) it also rewrites the 3-word crossfade groups (key 01: 65-67, 69-71 and mirrors) | High |
| 3 (3F68) | 0F89 | Complementary pair x and 32-x (key 01: words 41/42, 91/92; 45: 47/48; 84: 40/41; 06: 39/40) | High |
| 4 (3F69) | 0FB3 | Complementary pair (key 01: 36/37, 86/87; 45: 49/50; 84: 42/43; 06: 41/42) | High |
| 5 (3F6A) | 0FD2 | Four gains from interpolated tables 106D/107D/108D (key 01: words 32-35; 45: 34-37; 84: 33-36; 06: 32-35). Resolved in 13.3: the raw pot value A (0-255) gives position 3A/256 across four knots per gain; curves are in slider\_luts\_v44.json (s5) | Medium |
| 3F6D, 3F6E | 109D, 10CA | Fine predelay crossfade (M) and one more section | Low |

Corrected after reading the pot handler (06C8): sliders 1-4 take values 1-31 (raw pot >> 3, minimum 1), so the odd outputs at codes above 32 in the first sweep were out-of-range inputs. The exact tables over the real ranges are in slider\_luts\_v44.json.

### 12.6 Emulation architecture: DSP level, not ROM level

A DSP-level emulation is feasible and is the recommended route. The audio path is a fixed 100-step microprogram per 20 kHz sample (500 ns cycle) with 16-bit multiply-accumulate and a 14-bit circular delay memory; the 8080 only rewrites a few coefficients on slider moves and during modulation.

1. **Audio engine:** interpret the 128 microwords per sample: DMEM read at (position - offset), multiply by the coefficient, accumulate, write back.
2. **Control layer:** reimplement the 8080 logic as plain functions (curve table 0F4A, tables 106D/107D/108D, section grammar, complementary pairs, crossfade taps). This avoids shipping firmware. Alternative: embed a small 8080 core and the supplied ROMs for bug-for-bug behaviour; licensing needs checking.
3. **Validation:** the Python 8080 harness is the oracle; compare coefficient memory after each slider move against the reimplementation.

Blockers before the audio engine is exact: (a) the remaining microword control bits (byte A except the sign, byte B bits 1:0: memory write, accumulate/clear, saturate, converter and X-register I/O, halt) need decoding from program structure or the T&C/ARU schematics (manual pages about 96-101); (b) ARU scaling and rounding or overflow behaviour; (c) converter and transformer behaviour; (d) true 224X sample rate.

### 12.7 Open questions status

- **Closed:** program data and record layout; coefficient encoding; fractional delay (it exists, as a crossfade); version differences V4.3 to V4.4; program-key mapping.
- **Still open:** items (a)-(d) above; pot to slot assignment (slot 5 is the sixth pot, confirmed; the others are not); predelay (3F6D) effect, which changed no program words in my sweep; key 0E slider tables; modulation source confirmation; V2.2/V3.2 header grammar; sections A, L, M roles; 8080 firmware diffs between versions.

### 12.8 Service manual findings for the High Speed Processor

The 113-page manual has no T&C or ARU schematics. Pages 81-93 hold the DMEM, input and FPC schematics, 94-100 the 8080 board, and 101-113 the remote head, backplane, output board and power supply. The microword bits therefore have to be inferred from the program data plus the signal names below.

**HSP port map (8080 side, from the diagnostics page).** OUT 0 single-cycle, OUT 1 continuous run, OUT 2 halt immediately (halt = repeat program step 0), OUT 3 continue, OUT 5 clear the current-position counter, OUT 6 / OUT 7 write the low / high byte of the X register (OUT 7 also loads the bus test register). IN 0 and IN 1 read OFST0-13, XFER and CAS; IN 6 / IN 7 read the X register. IN 3 to IN 5 expose the decoded microword signals:

| Port | Bits 0-7 |
| --- | --- |
| IN 3 | SAT/, AS0, AS2, AS1, C3 xor C2, C1 xor C0 (inverted), S5 xor C4, ZERO |
| IN 4 | WA0, WA1, RA0, RA1, CSIGN, RD AD, MEM W, RESET/ |
| IN 5 | SDAA/, SDAB/, SDAC/, SDAD/, WR DA, WR XREG, RD XREG, RD RREG |

Reading: the ARU has a small register file with 2-bit write and read addresses, a result register (RREG), a coefficient with sign and six magnitude bits, add/subtract controls, a zero/clear control and a saturation flag. The audio converters, DACs (four channel strobes) and the X register are all read or written over the 16-bit digitized audio bus (DAB).

**Per-sample sequence.** A RESET microinstruction ends each 100-step sample. Its rising edge increments the 14-bit current-position counter, clears the FPC input cycle counter and starts a new A/D conversion. If SCYCLE is high when RESET runs, the processor halts.

**DMEM.** Sixteen 4116 dynamic RAMs give 16K x 16. Address = current position minus OFST (two's complement adder). Steps with no memory access run RAS-only refresh cycles, so a step either accesses memory or refreshes it. A write happens when MEM W is asserted as CAS falls.

**FPC.** Input: 12-bit mantissa plus 2-bit gain range becomes 16-bit two's complement by shifting 1 to 4 places. Output: 16-bit fixed point is converted to floating point with at most 3 shifts and an output gain code, with a double buffer and four channel strobes. Quantisation and gain-ranging noise therefore follow this converter, not a plain 16-bit DAC.

**8080 board.** RAM is 2114s at 3C00-3FFF (jumper options for 3800-3FFF and 3000-3FFF); the program memory is four byte-wide chips at 4000-41FF.

**Microword bit evidence (hypothesis).** In all 11 V4.4 programs only 41 distinct combinations of the top two offset bits, A bits 6:0 and B bits 1:0 occur.

- Top two offset bits = 3 with offset 16383 mark steps with no memory access; the offset is a don't-care.
- A bit 7 is the coefficient sign and B bits 7:2 the inverted magnitude (confirmed by the firmware).
- The most common memory-write step has top bits 0, A = x1000100 and coefficient 32 (unity), the pattern of storing the result.
- The most common memory-read step has top bits 1 and A = x1001100 or x1011101.

To finish the decode: assign the nine remaining bits to RA/WA, RD/WR source, AS and ZERO by fitting these \~40 patterns to program structure, then check against the diagnostics programs the 8080 loads (halt and XREG tests in ROM).

## 13. Implementation guide (sound first)

Build the reverb core from the extracted programs and slider tables, and finish decoding the microword bits before claiming sample-accurate output. Everything the sound depends on is in the data pack (lexicon224\_data\_pack.zip, delivered with this doc) except the microword opcode semantics, which are the one open blocker.

### 13.1 What is known and what is not

| Item | Status | Where |
| --- | --- | --- |
| Sample rate 20 kHz, 100 steps per sample, 500 ns step | known | 12.8, programs run words 28-127 |
| 7 V4.4 algorithm programs, 100 steps each, all delay offsets and coefficients | extracted | programs\_v44.json, steps\_v44.csv |
| Delay memory 16K x 16, offsets 14 bits (longest used 16383 samples = 819 ms) | known | 12.8 |
| Coefficient: sign + 6-bit magnitude, 32 = unity | known (firmware) | 12.3 |
| Slider to coefficient laws for 6 of 7 programs | extracted exactly from firmware | slider\_luts\_v44.json |
| Slider laws for key 0E | missing (loader spins in my emulator) | open |
| Microword bits A\[6:0\] and B\[1:0\] (register file, add/subtract, input/output, X register) | NOT decoded | 13.4 |
| Converter and filter behaviour (12-bit mantissa + 2-bit range in, float out) | known from manual | 12.8, 11.2 |
| Modulation source | probable (firmware bytes) | 12.4 |
| ARU rounding, overflow and saturation details | unknown | open |

### 13.2 Original presets

- **V4.4 has no stored presets.** The ROMs hold only the 7 algorithm records. Settings live in slider positions and battery RAM registers (the RAM diagnostic destroys them), so there is nothing like a factory bank to extract.
- **What does exist:** the program list. Select codes 01, 04 = key 01; 02, 10 = key 45; 08 = key 84; 20 = key 06; 09 = key 0C; 0C = key 1C; 21 = key 0E. That is the nine original programs on seven algorithms; their names are in section 3.
- **224XL V8.2.1** ships 21 named programs: Concert Hall, Bright Hall, Dark Hall, Rich Chamber, Room, Small Room, Chamber, Plate, CD Plate A, CD Plate B, Small Plate, Chorus&Echo, Res Chords, M Band Delay, Hall/Hall, Plate/Plate, Plate/Hall, Plate/Chorus, Rich Plate, Dark Chamber, Inverse Room. Mapping each name to one of the 12 XL algorithm records is not yet decoded. The raw program table is in constants\_and\_tables.json.
- **Plug-in presets** are a separate product and are not part of this analysis.

### 13.3 Using the slider tables

1. Load a program's 100 steps (words 28-127) from programs\_v44.json.
2. For each slider change, look up the slider's table (s1 to s5, e) for the program: each entry gives the new signed coefficient for each controlled word.
3. Sliders 1 and 2 interact (the firmware uses the difference), so use the s1s2 grid for words listed there.
4. Sliders 3 and 4 are complementary pairs (x and 32-x) on two words per channel; slider 5 is a piecewise-linear curve through four knots at raw pot 0, 85, 171 and 256 (tables 106D, 107D, 108D).
5. Slider values are 1..31 (raw pot >> 3, minimum 1); slider 5 uses the raw 0..255 pot value.
6. Mirror words (+50) carry the second channel and are already included.

### 13.4 Finishing the microword decode (the blocker)

The ROM's own diagnostics load known test microwords and read back the decoded signals, so the mapping can be derived without schematics.

1. Extract the (word, expected port value) pairs from diagnostic\_microword\_tests\_v44\_disassembly.txt. The tables sit at 1353, 1581, 1589 and 15FA; words are written downward from 41FF (table byte 0 lands in byte 3, the coefficient byte B) and the halted processor repeats word 127.
2. Confirmed from them: unused control bits are 1 (a NOP word is FF FF FF FF), so control signals are active low. OFST0-7 reads back the low offset byte unchanged.
3. Solve the A and B bit assignments against the signal list in 12.8 (register addresses, add/subtract, ZERO, SAT, RD AD, WR DA, X register, RREG).
4. Check the result by running each program and confirming the impulse response decays, the decay rate follows slider 5, and no step overflows.

**Fallback if the decode stalls:** build the reverb as a structural network from the extracted tables. Use each program's delay offsets as the delay lengths, the step coefficients as gains and the slider tables as controls, with the memory-read steps as taps and the unity-coefficient memory-write steps as stores. Mark it as inspired, not exact.

### 13.5 Sound-fidelity checklist

- Run the core at 20 kHz with 16-bit two's complement arithmetic and a 14-bit circular delay memory; keep the position counter shared by every access.
- Model the 12-bit mantissa plus 2-bit range input and the float output (at most 3 shifts) for the quantisation noise character; the 8 kHz input filter and output reconstruction are in 11.2.
- Two-tap interpolation at offsets N and N-1 gives fractional delay; the 8080 rewrites its two weights (12.4).
- Add the modulation only after the fixed core matches; the firmware-bytes source is probable, not confirmed.
- Validate with impulse response, decay time per slider position, and echo density before comparing by ear.

### 13.6 Reference projects

The BlueBox project (PedalPCB forum, GitHub jimbattin/bluebox) emulates the 224XL's 8080 board and the ARU and runs the original V8.21 ROMs, so it models the ARU, so it must implement the microword semantics for the XL (inferred, not verified). Its repository could not be read here (the fetch was refused), so I did not use its code. Its licence is also unstated, so check it before reusing anything.

### 13.7 Using BlueBox as a verification oracle

BlueBox (github.com/jimbattin/bluebox, announced on the PedalPCB forum) runs the original 224XL V8.21 firmware with an emulated 8080 board and ARU, and its emu/ folder renders an input wav to an output wav. It needs the 8 NVS and 3 SBC images you already have, in a roms/224XL\_v8\_21/ folder. Use it to check the decode, not as the product core.

1. **Get it running.** Download the repository as a zip (or clone it) and install its Python dependencies with uv. Place the V8.21 images under roms/224XL\_v8\_21/. I could not read the source here because the fetch was refused, so nothing below depends on its internals.
2. **Render test signals** through each XL program: a unit impulse at full scale, a -20 dBFS 1 kHz burst, and white noise. Save the impulse responses. Use the same slider settings every time and record them.
3. **Cross-check the decode.** XL records are a faster sibling of the 224 microcode, so run my decoded interpreter on the XL records first. Compare per-sample outputs against BlueBox on the impulse and the burst. A correct opcode decode should match to the last bit or to small, explainable rounding differences.
4. **Then move to the 224 programs.** Once the interpreter matches the XL, apply the same decode to the V4.4 records and compare decay time, echo density and spectrum with the 224XL equivalents. The XL programs are not identical to the V4.4 ones, so expect similar, not equal, behaviour.
5. **Record the unknowns it can answer:** ARU rounding and overflow, saturation behaviour, converter quantisation and the true modulation source. Each is a visible difference between an exact and an approximate interpreter.
6. **Licence.** The author states there is no copyright or licence beyond one vendored header, which is not the same as a permissive licence. Treat the code as reference only, and do not copy it into your plugin. Running it on your own ROMs for measurement is a separate matter from distributing anything derived from it or from the ROMs.

**Acceptance tests for the decoded interpreter**

| Test | Pass condition |
| --- | --- |
| Impulse through an XL program | matches BlueBox within a stated tolerance, ideally bit-exact |
| Decay time versus slider 5 | RT60 moves smoothly and monotonically across the knots in 13.3 |
| Silence in | zero out, no limit cycle |
| Full-scale impulse | no unexplained overflow; saturation flag matches BlueBox |
| V4.4 program 01 | stable, decaying, echo density builds as expected for a hall |

### 13.8 Additional facts learned since sections 3 to 11

- **ROM mapping (V4.4):** code ROM1-3 at 0000-17FF, program ROM4 at 1800-1FFF and ROM5 at 2000-27FF, four 512-byte records each. The diagnostics describe the ROM checksum by bit lane across the four ROM numbers.
- **Program start:** the T&C counter runs steps 28-127 (100 steps, 20 kHz at 500 ns). Words 0-27 hold the header and are not executed in the sample loop. This is inferred from the 100-step count and the identical start and end words, not read from a schematic.
- **Control polarity:** unused control bits are 1; a NOP word is FF FF FF FF. The halted processor repeats a fixed step (word 127 in the diagnostic tests).
- **224XL image layout (V8.21, 32 KB across 8 NVS chips):** 8080 code mapped at 8000h, UI text starting at image offset 2000h, a program table near 2440h, page tables near 24AEh, and algorithm records at 3800h onward (stride 02AAh, 12 records).
- **224XL features seen in the strings** that have no V4.4 equivalent: banks and registers with recall, clear and label entry; store, verify and recall of banks on tape; dynamic decay, mode enhancement and decay optimisation toggles; output mute; internal tuning; reverse-stop delay; slope and PD levels; size and gate pages. Treat them as XL-only unless a V4.x ROM shows otherwise.
- **Key 0E:** its loader does not complete in my emulator (it loops waiting on a hardware read), so its slider tables are missing from the data pack.
- **Fractional delay (corrected):** the older statement that the 224 has no fractional-delay hardware is wrong; see 12.4. Section 8's "all-pass interpolation" as an added feature should be read with that correction.

### 13.9 Recommended order of work

1. Get BlueBox running on the V8.21 ROMs and render the test signals (13.7).
2. Decode the microword bits using the diagnostic test words and BlueBox output as ground truth.
3. Build the 100-step interpreter and verify it bit-for-bit against BlueBox on the XL records.
4. Run the V4.4 programs through it, drive them with the slider tables in the data pack, and tune the converter and filter model against section 11.2.
5. Fill the remaining gaps: key 0E sliders, predelay, the modulation source.
6. Only then add UI and preset handling; the nine original programs and the XL program names are already listed in 13.2.
