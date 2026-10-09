# Vintage Synth Emulation — General Implementation Compendium

Oct 4, 2026 · @Søren

A cross-instrument reference for building circuit-informed emulations of vintage synthesizers and their effects chain: what kinds of blocks exist, which algorithm suits each, what the trade-offs are, and how to calibrate the result against recordings and other emulators.

## 1. Purpose, scope, and how to read this document

This compendium generalises the method used in the project's per-device compendiums (TB-303, MXR Distortion +, Boss BD-2, TC Electronic Magus Pro) into a reusable playbook. It does not describe one instrument. Per-instrument facts (serial-number revisions, prices, anecdotes, exact component values) belong in that instrument's own compendium; this document says how to find, grade and use them.

**The central claim, supported across all four project compendiums and the literature read for this one:** an emulation is accurate when it reproduces the *interactions* between blocks (loading, current-domain control summing, distributed nonlinearity, persistent capacitor state), not when each block looks right in isolation. Linear frequency-response agreement is necessary but tells you little about nonlinear fidelity (Section 6.4).

**Confidence legend** (extended from the companion documents):

| Tag | Meaning |
| --- | --- |
| *(sourced)* | Stated by a source read for this document or its companions (Section 16) |
| *(computed, project)* | Derived and numerically verified in a project compendium or the TB-303 filter audit |
| *(inferred)* | Follows from sourced circuit facts by standard analysis, not stated by a source |
| *(approx.)* | An engineering estimate or a rule of thumb; calibrate before relying on it |
| *(general knowledge, unverified)* | Widely repeated in the field but not re-checked against a source this session; verify before using as a constant |

**How a per-instrument compendium should be laid out** (the ten-part template used by this project, restated so this document can refer to it):

1. History, production revisions keyed to serial numbers, prices.
2. Anecdotes and famous uses, each sourced or flagged unverified.
3. Interface: every control with range, units, positions, taper and storage or quantisation.
4. Front-panel sketch and signal flow (drawn).
5. Components per block with values and a confidence tag per value.
6. Quirks, per-unit variation, calibration and known faults.
7. Existing emulations, software and hardware, with a quality judgement.
8. Emulation architecture, with 2–3 algorithm options per block compared on accuracy, CPU and flexibility.
9. Open questions and a validation test matrix.
10. Sources actually read, with links.

Rules carried into every section below: estimates are marked *(approx.)*; myths found in other sources are corrected explicitly (Section 14); faithful hardware behaviour is kept separate from added plugin features; no code examples.

## 2. Lessons carried over from the project's compendiums

The four device compendiums and the TB-303 filter audit already contain the hard-won rules; each row below states one, where it was learned, and how it generalises.

| Lesson | Where it was learned | General rule |
| --- | --- | --- |
| Control signals sum as currents, then pass an exponential converter | TB-303 Env Mod bias transistor Q9 and the Q10/Q11 log/antilog pair *(sourced, Service Notes)* | Never add envelopes to cutoff in Hz; sum in the circuit's own domain, then apply the converter |
| One knob can drive two destinations | TB-303 Resonance is a dual-gang 50 kΩ pot; gang 2 shapes the Accent Sweep *(sourced, parts list)* | Model ganged pots as one control with several destinations; check every pot's part number for `×2` |
| Capacitor state is never reset at note boundaries | TB-303 Accent Sweep 1 µF cap gives rising peaks on consecutive accents *(sourced, Whittle)* | Every RC in a control path is persistent state; only reset what the hardware resets |
| Ladder stages load each other | TB-303 diode ladder lacks inter-stage buffers *(sourced, Stinchcombe via secondary)* | Solve coupled stages together; independent one-poles with post-saturation miss the loading |
| Loop gain is the first thing to check when a filter misbehaves | Audit: shipped feedback ceilings were about 2× the critical gain k = 17 *(computed, project)* | Derive the linearised characteristic polynomial and its critical gain before tuning anything by ear |
| Constants from another emulator carry that emulator's discretisation | Audit: Open303's k polynomial compensates its explicit discretisation; transplanting it into a trapezoidal solver self-oscillates *(computed, project)* | Copy behaviour targets, never internal constants, from other emulators |
| Finite op-amp bandwidth is audible at high gain | MXR: the 741's GBW turns a plateau into a \~1.8 kHz hump at max gain *(computed, project)* | Model GBW and slew where gain × bandwidth approaches the audio band |
| tanh is not universal | BD-2: the JFET pair reaches a hard limit at a finite input (√2·V\_ov) *(computed, project)* | Derive each device's own curve; tanh is a BJT differential-pair law |
| Shunt and feedback diode clippers share one solver | Magus Pro: a feedback clipper reduces to the shunt equation by substitution *(computed, project)* | Build one robust diode-node solver (Wright omega seed + Newton) and reuse it with the right Thevenin source |
| The last hard nonlinearity dominates aliasing | MXR: removing the diode stage cut aliasing far more than removing the op-amp saturator *(computed, project)* | Put antialiasing effort (oversampling or ADAA) on the final hard nonlinearity first |
| Stiff nodes need oversampling for accuracy, not only for aliasing | MXR clipper node τ = 5 µs vs a 20.8 µs period at 48 kHz *(computed, project)* | Compare every node time constant with the sample period before choosing the rate |
| Published analyses contain errors | MXR: an arithmetic error and a mis-stated corner in a widely read analysis; BD-2: diode count and part corrected by the factory schematic | Re-derive every number you can; prefer the factory schematic and service notes |
| AI-written references invent plausible numbers | TB-303 reference file: qualitative advice sound, several specific values unsourced or borrowed from the Devil Fish mod | Treat such files as hypotheses; tag each number before use |
| Evidence grade sets document style | BD-2 is schematic-grounded; Magus Pro is analogy-grounded (RAT family) | State the evidentiary position at the top of each compendium |

The project's pedal research queue already applies the last rule: confirm a schematic exists before choosing between a BD-2-style and a Magus-style compendium.

## 3. Taxonomy of vintage-synth blocks and component categories

Every vintage synth decomposes into the same handful of block categories; the emulation strategy is chosen per category, then refined per instrument. Instrument examples below are illustrative *(general knowledge, unverified)* unless a project compendium covers them.

### 3.1 Block categories

| Category | Typical vintage forms | What carries the sound | Default strategy | Upgrade path |
| --- | --- | --- | --- | --- |
| Control and digital logic | Analog keyboard CV, CPU scanning, sequencers (TB-303: 24 ppqn clock, 6-bit pitch DAC *(sourced)*), ADC-scanned sliders | Timing, gate length, DAC resolution, scan-rate quantisation, glide RC | Emulate at control rate with the hardware's own clock and quantisation | Cycle-level CPU or firmware emulation where the ROM is available |
| Oscillators | VCO (expo converter + saw core), DCO (digital divider + analog integrator), early digital | Reset shape, waveshaper curves, pitch-dependent duty, coupling high-pass, drift | Band-limited (BLEP family) core plus the analog waveshaper and coupling network | Oversampled continuous-time core model with a reset comparator |
| Waveshapers | Comparator square, transistor shapers, sub-octave flip-flops, wavefolders | Curve shape, asymmetry, amplitude ratios between waves | Static memoryless curve, antialiased (ADAA) or oversampled | Device-level model inside the oscillator solve |
| Filters | Transistor ladder, diode ladder, OTA cascade, SVF, Sallen-Key, Steiner-Parker, LPG | Loading, distributed saturation, feedback-path shaping, cutoff law | ZDF/TPT structure with distributed nonlinearity, solved implicitly | Full nodal (DK) or WDF model from the schematic (Section 6) |
| Envelopes and LFOs | RC envelopes, comparator-switched charge/discharge, CEM/SSM chips, firmware envelopes | Curve (exponential towards an overshooting target), retrigger from current value, release on gate-off | RC state variables with hardware thresholds and targets | Transistor-level switch and comparator model |
| VCAs | OTA (CA3080-class), exponential VCA chips, BA662-class transconductance amps, vactrol LPGs | Control law (linear or exponential), bleed, soft ceiling, distortion at high control current | Gain law plus soft ceiling, control current summed before the law | OTA with tanh input and current output; vactrol as a nonlinear RC with asymmetric time constants |
| Mixers and coupling | Summing op-amps, coupling caps, output stages | Low- and high-frequency corners, level-dependent saturation | Linear filters from component values | Include op-amp rails and GBW where gain is high |
| Effects | BBD chorus and delay, spring reverb, tape echo, drive pedals | Companding, clock-dependent filtering, dispersion, wow and flutter, clipping | Block-specific models (Section 9) | Circuit-level models of the support circuitry |
| Power and environment | Regulators, capacitance multipliers, batteries, thermal drift | Headroom, sag, drift, noise | Supply voltage and temperature as hidden parameters | Dynamic rail model coupled to load current |

### 3.2 Component categories

| Component | What to model | Default model | When it matters |
| --- | --- | --- | --- |
| Resistors, capacitors | Value, tolerance; capacitor dielectric only for exotic cases | Ideal values; tolerance as a seeded perturbation | Always for corners; tolerance for per-unit variation |
| Potentiometers | Taper curve, total value, wiper loading, ganging, end-stop resistance | Measured or datasheet taper curve mapped from knob angle | Whenever a pot sits inside a network (loading changes corners, see MXR volume pot) |
| Diodes | I\_s, n, series resistance, mismatch | Shockley with Wright-omega seed + Newton | Clippers, ladder elements, envelope switches |
| BJTs | Differential-pair law, V\_T temperature dependence, β, mismatch | Ebers–Moll reduced to tanh for matched pairs | Ladders, expo converters, waveshapers |
| JFETs | Square-law pair, I\_DSS and V\_P spread | Closed-form square-law pair (BD-2) | JFET gain stages and buffers |
| Op-amps | GBW, slew rate, rail swing, recovery | Ideal plus a GBW pole, slew clamp and asymmetric rails | High-gain stages; slow parts (741, LM308) |
| OTAs | tanh input stage, current output, bias-current leakage | tanh(v/2V\_T) × I\_abc | OTA filters and VCAs |
| Custom ICs (CEM, SSM, BA662) | Whatever the datasheet or reissue datasheet documents | Behavioural model from datasheet curves | Filters, VCAs, envelopes in 1980s polysynths |
| Vactrols | Asymmetric attack/decay, light-to-resistance law, memory | Nonlinear first-order system with two time constants *(approx.)* | Buchla-style lowpass gates |
| Tubes and transformers | Triode/pentode laws, grid current, magnetic hysteresis | Published triode models; hysteresis model for saturation | Mostly effects and output stages; rare in synth voices |

## 4. Generic signal flow and emulator architecture

Every instrument in this project maps onto the same three-layer architecture: a control layer at host rate, one oversampled voice core where all coupled nonlinear blocks are solved together, and an outer ring of effects and hidden calibration parameters.

&#91;embedded content: generic emulator architecture · 3 layers\]

Control signals enter the core only by being summed as currents or voltages at the circuit's own summing nodes; the dashed link shows calibration parameters setting every block's hidden values rather than processing audio.

**State carried per voice, never reset except where the hardware resets it:** oscillator phase, post-glide pitch CV, every envelope and accent capacitor, every filter integrator, coupling-capacitor voltages, op-amp slew states, and any supply or thermal state. The TB-303 compendium's per-voice state table is the worked example.

**Front-panel sketch convention for instrument compendiums.** Draw the panel as a grid of controls in their physical positions, each labelled with its range and taper, and draw the signal flow beside it with every control placed on the block it acts on. Ganged controls get one symbol with two arrows.

## 5. Oscillators: algorithm options compared

The oscillator is rarely what makes an emulation sound wrong, but its *analog surroundings* often are: the waveshaper that derives secondary waves, pitch-dependent shapes, coupling high-passes and relative amplitudes into the filter. The TB-303 compendium shows all four (pitch-dependent square duty ≈45% to ≈70%; both waves look high-passed around 80–115 Hz; saw and square drive the filter differently) *(sourced)*.

### 5.1 Core waveform generation

| Option | Accuracy | CPU | Flexibility | Notes |
| --- | --- | --- | --- | --- |
| A. Trivial (naive) waveform at host rate | Ideal shape, heavy aliasing at high pitch | Lowest | High | Acceptable only for bass-register monosynths; the audit found it harmless at 303 pitches but flagged it *(computed, project)* |
| B. BLEP family: minBLEP, polyBLEP, integrated B-spline corrections | Near-ideal discontinuities; Välimäki et al. report perceptually alias-free sawtooth up to a 7.8 kHz fundamental at 44.1 kHz with an integrated third-order B-spline correction ([source](https://koasas.kaist.ac.kr/handle/10203/201388)) | Low (2–4 samples corrected per edge) | High: hard sync, PWM, FM all workable | Default for most instruments; the correction is added at each discontinuity, so any reset-based core benefits |
| C. Continuous-time core model (integrator, reset comparator, finite reset time) run oversampled | Captures reset slope, comparator delay, pitch-dependent shape | Medium to high | Medium | Upgrade path when the reset shape is audible (fast VCO resets, sync quirks) |
| D. Wavetables / BLIT (band-limited impulse train) | Exact band-limiting for static shapes | Low per sample, memory for tables | Low for shapes that change with pitch or CV | BLIT with low-order fractional-delay filters is efficient for classic shapes ([Nam et al. 2010](https://ccrma.stanford.edu/papers/efficient-antialiasing-oscillator-algorithms-using-low-order-fractional-delay-filters)); poor fit when the analog waveshaper changes the shape with pitch |

**Recommendation.** Option B for the core, then model the analog waveshaper and coupling network explicitly (Section 5.2), all inside the oversampled region when the next stage is a nonlinear filter. Option C only where a measurement shows the reset shape matters.

### 5.2 What sits around the core

| Behaviour | Model it as | Avoid |
| --- | --- | --- |
| Secondary waves derived from the primary (square from saw) | The actual shaper (comparator or transistor stage) driven by the primary | An independent ideal square with a fixed duty |
| Pitch-dependent duty or shape | A shaper whose threshold or bias depends on the pitch CV | A constant duty (the 303's "46%" myth, Section 14) |
| Coupling into the filter | The coupling-capacitor network with the filter's input impedance | A bolted-on high-pass at an invented corner |
| Relative wave amplitudes and DC offsets | Measured or derived ratios, not normalised to ±1 | Post-filter gain compensation |
| Pitch law, scale and offset trims | 1 V/oct expo converter with scale/offset trim parameters | Perfect 12-TET mapping in a "faithful" mode |
| Drift | Slow filtered noise on the pitch CV plus a temperature term in V\_T *(approx.)* | Per-note random detune with no time correlation |
| Phase continuity | Free-running phase, never reset on retrigger or slide unless the hardware does | Phase reset per note in a VCO model |

### 5.3 DCOs and early digital oscillators

A DCO is a digital counter that resets an analog integrator; pitch is exact, but the integrator's amplitude depends on how well the charging current tracks frequency *(general knowledge, unverified)*. Emulate the counter exactly (including its clock and divider quantisation, which sets the pitch grid) and the integrator as an analog ramp with an amplitude-compensation law. Early digital oscillators (wavetable chips, low-bit DACs) are emulated bit-exactly at their native rate, then passed through a model of their reconstruction filter; their aliasing is part of the sound and is *faithful*, not a defect to remove.

## 6. Filters: topologies and algorithm options compared

The filter is where emulations succeed or fail. The best default is a topology-preserving (TPT) zero-delay-feedback structure that keeps the nonlinearity *distributed* across the stages where the circuit has it, solved implicitly at 2–8× oversampling. A 2026 SPICE-referenced benchmark found that preserving distributed placement mattered at least as much as the exact saturator law ([Oyama, DAFx26](https://www.dafx.de/paper-archive/2026/papers/DAFx26_paper_29.pdf)).

### 6.1 Topologies and what makes each one sound like itself

| Topology | Examples | Character-defining mechanisms | Modelling priorities |
| --- | --- | --- | --- |
| Transistor ladder (buffered stages) | Moog ladder | tanh saturation per differential pair, 1 input + 4 stage nonlinearities ("1+4"), self-oscillation, bass loss with resonance | Distributed tanh, input pair, feedback gain mapping, passband compensation |
| Diode / transistor-as-diode ladder (unbuffered) | TB-303 *(sourced)*, EMS VCS3 (modelled as a nonlinear filter network in the literature, per the reference list of [arXiv 2111.05592](https://arxiv.org/pdf/2111.05592)) | Inter-stage loading, unevenly spaced poles, extra coupling poles inside the loop, limited or no clean self-oscillation | Coupled state-space solve, coupling HPF inside the feedback loop, critical-gain check |
| OTA cascade | CEM3320-based and IR3109-based polysynths *(general knowledge, unverified)* | OTA tanh input stages, buffer saturation, bias-current leakage, chip-specific feedback | Per-stage OTA law, feedback path through the chip's resonance VCA |
| State-variable (SVF) | Two-integrator-loop designs; Chamberlin is the classic digital form | Simultaneous LP/BP/HP outputs, damping-path nonlinearity, integrator saturation | TPT SVF (Zavalishin chapter 4), nonlinearity in the integrators or damping path |
| Sallen-Key / Korg35-style | MS-20 family *(general knowledge, unverified)* | Positive feedback through a nonlinear element, aggressive screaming resonance | Nonlinear element in the feedback path, solved implicitly (port-Hamiltonian and ZDF treatments exist, see 6.2) |
| Steiner-Parker, cascaded "CAT"-style variants | Steiner-Parker Synthacon; Octave CAT studied by Werner and McClellan ([DAFx-20](https://dafx.de/paper-archive/details/tKYhnWc19H9rsRWpx9vyog)) | Diode-steered multimode response, cascaded SVF with global feedback | Derive from schematic; SVF-cascade generalisation where it applies |
| Lowpass gate (vactrol + filter/VCA) | Buchla 292 lineage (D'Angelo thesis covers a Buchla LPG model, [Aalto](https://aaltodoc.aalto.fi/items/08b88f1e-4cc4-4dee-a53f-8aa63c5c5da4)) | Coupled amplitude and brightness, asymmetric vactrol memory | Vactrol dynamics drive both the filter and the gain from one state |

Topologies named *(general knowledge, unverified)* above need their schematic confirmed in that instrument's compendium before they drive design choices.

### 6.2 Algorithm families

| Family | How it works | Accuracy | CPU | Flexibility | Sonic quality when done well |
| --- | --- | --- | --- | --- | --- |
| A. Linear digital prototype (biquads, bilinear transform) + output saturator | Fit a transfer function, saturate at the boundary | Correct small-signal shape only; wrong under drive | Very low | High | Clean and "digital"; resonance does not compress or shift |
| B. Explicit nonlinear cascade (Huovilainen 2004) | Ladder ODE solved by Euler; one-pole sections with embedded tanh; tuning correction ([DAFx-04](https://dafx.de/paper-archive/details/UervgvkeeDC1a4sWDluM4Q)) | Very good for the Moog ladder in the 2026 benchmark (closest of five to SPICE on linear and harmonic metrics) *(sourced)* | Low; needs ≈2× oversampling and tuning polynomials | Medium: correction polynomials are topology-specific | Classic, lively ladder sound |
| C. TPT / ZDF with boundary saturation (tanh at input and feedback only) | Zavalishin structure, four linear sections | Linear response fine; nonlinear behaviour diverges strongly | Low | High | Pleasant but not ladder-like when driven; in the benchmark, JUCE's version of this topology missed the resonance-peak shift by 427 cents and did not sustain self-oscillation at the test setting *(sourced)* |
| D. TPT / ZDF with distributed nonlinearity, Newton or fixed-point solve | Implicit equation per sample; saturator in every stage | Highest among real-time structural models; distributed tanh core sat at 0.05% of H1 on the benchmark's spectra metric *(sourced)* | Medium (1–4 Newton iterations per sample) | High: change the saturator by changing a function and its derivative | Most faithful drive, cutoff shift and self-oscillation behaviour |
| E. Delay-free-loop explicit methods (D'Angelo & Välimäki 2014) | Removes the implicit solve with an explicit delay-free-loop technique ([thesis](https://aaltodoc.aalto.fi/items/08b88f1e-4cc4-4dee-a53f-8aa63c5c5da4)) | Close to SPICE in the benchmark (self-oscillation sustain near SPICE) *(sourced)* | Low to medium | Medium | Close to D |
| F. Circuit-level: nodal DK / state-space, WDF, port-Hamiltonian | Equations generated from the schematic; nonlinearities solved by Newton, tables, or non-iterative schemes | Highest attainable; limited by device models and unknown component values | High (tables reduce it) | Low per topology, but generic tooling | Most faithful; can capture loading the structural models miss |
| G. Black-box / grey-box learned models (RNNs, differentiable DSP) | Learn the mapping from input and controls to output | Very good inside the training distribution; can fail outside it | Medium to high | Low for knob coverage unless conditioned | Can capture unknown circuitry; risk of artefacts at unseen settings |

**Port-Hamiltonian and non-iterative schemes** give guaranteed-stable simulations without Newton iterations; DAFx-21 included applications to the Korg35 and Moog four-pole VCF ([proceedings listing](https://pub.mdw.ac.at/pubmdw/publication/b7503769-3fc2-4827-842e-e8df5a8cbaa6)). Treat them as an alternative to D when worst-case CPU must be bounded.

### 6.3 Discretising the linear skeleton

The trapezoidal (bilinear) integrator in TPT form is the standard: it keeps the analog topology, so cutoff modulation behaves like time scaling and cannot destabilise a filter whose cutoff gains precede the integrators ([Zavalishin, ch. 2–3](https://www.native-instruments.com/fileadmin/ni_media/downloads/pdf/VAFilterDesign_2.1.0.pdf)). Two caveats from the project: forming polynomial coefficients and bilinear-transforming them in floating point fails when time constants span many decades (MXR); and a feedback gain tuned for one discretisation is wrong for another (audit). Möbius-transform one-step methods and conformal maps were proposed in 2021 to reduce trapezoidal warping and improve non-oversampled models ([DAFx archive](https://dafx.de/paper-archive/search?p=137)).

### 6.4 Measured lessons from the 2026 ladder benchmark

Oyama compared five implementations against one SPICE ladder (generic NPN, 68 nF capacitors) using swept-sine, self-oscillation, cutoff-shift and harmonic tests ([DAFx26](https://www.dafx.de/paper-archive/2026/papers/DAFx26_paper_29.pdf)). Findings that generalise:

| Finding | Implication for any emulator |
| --- | --- |
| All models agreed linearly within a fraction of a dB, yet differed widely in harmonic structure | Linear plots cannot validate a nonlinear filter; include drive, cutoff-sweep and self-oscillation tests |
| Collapsing nonlinearity to the boundaries (input + feedback) raised harmonic error several-fold over the distributed core | Keep saturation inside the stages that have it in the circuit |
| Swapping tanh for atan or a soft clip shifted the harmonic balance even with distributed placement | Use the device's own law; do not substitute "a nicer saturator" in a faithful mode |
| When only some stages could stay nonlinear, keeping the earlier stages nonlinear was closer to SPICE | For a CPU-reduced mode, linearise the later stages first |
| Huovilainen's model sustained self-oscillation slightly low because its resonance-correction polynomial was omitted | Small compensation terms change onset thresholds; verify onset explicitly |

The benchmark's own limits: its SPICE reference was not checked against hardware and omits tolerances, mismatch and thermal drift *(sourced)*. It establishes structural lessons, not absolute targets for any specific instrument.

### 6.5 Feedback, resonance and cutoff law

- **Resonance is a loop gain, not a Q.** Derive the linearised characteristic polynomial and its critical gain first (17 for the 303-style ladder ODE in the audit *(computed, project)*), then map the knob onto a fraction of that, with the hardware's own skew.
- **The feedback path has its own filtering.** Coupling high-passes inside the loop produce "resonance steals bass except near a low-frequency hump" (TB-303) and move the critical gain with cutoff.
- **Define the cutoff label.** Decide whether the Hz label means the −3 dB corner, the resonance peak or the hardware knob law; the audit found the same label meant different things in two implementations.
- **Passband compensation is part of the circuit or part of the plugin.** If the hardware loses level with resonance, a faithful mode must too; any compensation is an added feature (Section 14).

## 7. Nonlinear components: device laws and solvers

Each nonlinear device has its own law, and the law plus its circuit context (what source impedance drives it, what capacitance hangs on it, whether it sits in a feedback loop) decides the solver. The table gives 2–3 options per device class.

| Device and context | Option 1 (default) | Option 2 | Option 3 (highest fidelity) | Notes and evidence |
| --- | --- | --- | --- | --- |
| Diode pair, shunt or feedback clipper | Trapezoidal ODE step; Wright-omega closed-form seed + 1–2 Newton steps. Verified to machine precision in 2 steps *(computed, project)* | Wave digital filter diode model (Werner et al.), natural for asymmetric and multi-diode branches | Full nodal model including junction capacitance and series resistance | Same solver covers shunt and feedback clippers by substitution (Magus Pro). Fit I\_s and n to measured curves; some circulated SPICE lines are implausible (MXR: a 1N34 line gives ≈0.87 V at 1 mA) |
| BJT differential pair (ladders, OTAs) | tanh(v / 2V\_T) | tanh with mismatch offset and temperature-dependent V\_T | Ebers–Moll with β and parasitic resistance | Distributed per stage (Section 6.4) |
| JFET differential pair | Exact square-law closed form with a hard limit at √2·V\_ov *(computed, project)* | Piecewise fit to measurement | Full three-terminal JFET model | Same-bin I\_DSS and V\_P spread can move the knee by an order of magnitude (BD-2) |
| Op-amp in a gain stage | Linear stage + asymmetric bias-referenced saturator | Add a GBW pole and a slew clamp | Macromodel (input gm → compensation integrator → clamped output), solved with the feedback network | 741: slew 0.5 V/µs, GBW-limited hump at max gain (MXR); LM308: ≈0.3 V/µs (Magus/RAT) |
| OTA (VCA or filter stage) | I\_out = I\_abc · tanh(v / 2V\_T) | Add output-buffer saturation and bias leakage (control bleed) | Transistor-level OTA | Leakage creates the "VCA bleed" heard at zero envelope |
| Exponential converter | Ideal 1 V/oct with scale/offset trims | V\_T temperature dependence and tempco compensation *(approx.)* | Matched-pair mismatch + thermal time constant | Sets pitch tracking and filter key tracking |
| Transconductance VCA chips (BA662 class) | Gain law from control current + soft ceiling | Datasheet curve fit (V662A reissue datasheet for the BA662) *(sourced)* | Transistor-level model of the cell | Raw multiply is an oversimplification at high control current (TB-303) |
| Vactrol | Two-time-constant nonlinear first-order system *(approx.)* | Light-history-dependent model | Measured per-part model | Shapes LPG "plonk"; asymmetric and level-dependent |
| Triode / transformer (effects, output stages) | Static curve + coupling-capacitor bias shift | WDF triode models (D'Angelo et al., listed by [Aalto VA group](https://www.aalto.fi/en/department-of-signal-processing-and-acoustics/virtual-analog-synthesis-and-audio-effects)) | Grid-current and hysteresis models | Rare in synth voices; common in effects chains |

### 7.1 Choosing a solver for coupled nonlinearities

| Situation | Best solver | Why |
| --- | --- | --- |
| One nonlinearity, one state (diode node) | Closed form + Newton polish | Exact, cheap, branch-free |
| Several nonlinearities in one loop (ladder) | Newton on the stacked system, seeded from the previous sample | Converges in 1–4 iterations at oversampled rates *(approx.)* |
| Many nonlinearities, fixed topology, CPU-bound | Nodal DK with precomputed tables of the nonlinear core | Moves the solve offline; memory cost grows with dimension |
| Need a hard CPU ceiling or provable stability | Port-Hamiltonian non-iterative schemes | No iteration count variance |

**Nonlinearities that need memory.** Coupling-capacitor bias shift after asymmetric clipping, op-amp saturation recovery and supply sag are all state; a memoryless saturator cannot reproduce them (MXR §6.4).

## 8. Envelopes, LFOs, VCAs, CV paths, sequencers and stored or quantised controls

Control paths decide feel. Most "it sounds right on a held note but wrong in a pattern" complaints trace to envelope retrigger, state persistence, gate timing or control quantisation, not to the filter.

### 8.1 Envelope and LFO options

| Option | Accuracy | CPU | Flexibility | Use when |
| --- | --- | --- | --- | --- |
| A. Idealised ADSR with exponential segments | Shape roughly right; retrigger and thresholds wrong | Negligible | High | Never for a faithful mode |
| B. RC state model: capacitor charging towards a target through a resistor, switched by comparator thresholds and gate logic | Reproduces overshoot targets, retrigger from current value, gate-off behaviour | Negligible | Medium | Default for analog envelopes |
| C. Circuit-level model of the switch transistors and comparator | Captures non-ideal switching and coupling into other CV paths | Low | Low | When envelopes interact electrically (TB-303 accent paths) |
| D. Firmware emulation (microprocessor envelopes) | Bit- and timing-exact if the ROM logic is known | Low | Low | CPU-generated envelopes and LFOs in 1980s polysynths |

LFOs follow the same pattern: an integrator-and-comparator triangle core (option B-like) or firmware tables (option D), plus any delay or fade circuit.

### 8.2 CV summing and VCAs

Sum control voltages or currents at the node where the circuit sums them, then apply that node's law (exponential converter, OTA bias current, VCA control current). The TB-303's Env Mod, Accent Sweep and VCA accent path are the worked example *(sourced)*. Every RC in these paths is persistent state.

### 8.3 Documenting and emulating each control

For every control the instrument compendium must record range, units, positions, taper and storage. Then the plugin reproduces it in four layers:

| Layer | What to capture | Faithful emulation | Common error |
| --- | --- | --- | --- |
| Physical | Pot value, taper (linear, log/audio, reverse-log, custom), ganging, detents, switch positions | Store the knob as normalised mechanical rotation; map through the measured or datasheet taper to a resistance or wiper fraction | Mapping the knob directly to Hz or dB |
| Electrical | Where the wiper sits in the circuit and what it loads | Recompute the affected network from the resistance (MXR: one pot moves gain, bass corner and treble corner together) | Treating the pot as a gain after the network |
| Digitisation (if any) | ADC resolution, scan rate, firmware smoothing or hysteresis | Quantise at the hardware's bit depth and update at its scan rate | Smooth float parameters that remove audible stepping the hardware has |
| Plugin | Host automation resolution, MIDI 7/14-bit mapping, smoothing | Map host values onto the mechanical rotation; smooth only to remove zipper noise the hardware does not have | Letting MIDI's 128 steps replace the hardware's own steps |

**Taper approximations.** Audio (log) tapers are usually two-segment approximations, not true logarithms; a reverse-log pot runs the other way (MXR reissue Distortion pot *(sourced)*). Use the part's datasheet curve where it exists; otherwise measure resistance at 11 rotation points *(approx. procedure)*.

### 8.4 Sequencers and timing

Emulate the clock, the gate-on fraction and the slide logic at the hardware's resolution (TB-303: 24 ppqn clock; gate ON:OFF 3.5:2.5 of a 6-pulse step; slide RC ≈60 ms; slide does not retrigger envelopes or reset phase *(sourced)*). Run this layer at control rate with sample-accurate event placement; jitter only if the hardware has it and only in faithful mode.

## 9. Effects and delay-line blocks

Built-in effects (BBD chorus, spring reverb) and the pedals used with synths are part of the recorded sound, so a reference recording often includes them. Model them as separate blocks with their own rates, and keep them switchable so calibration can isolate the voice.

| Block | Character-defining mechanisms | Option 1 | Option 2 | Option 3 | Evidence |
| --- | --- | --- | --- | --- | --- |
| BBD delay / chorus / flanger | Clock-rate sampling, anti-alias and reconstruction filters, compander, transfer inefficiency, clock noise | Fractional digital delay + fixed filters + compander curve | Variable-rate model of the BBD itself, using the surrounding filters to avoid extra interpolation | Full circuit model of filters, compander and BBD nonlinearity | Raffel & Smith model each part from analysis and measurement ([DAFx-10](https://dafx.de/paper-archive/details/JhVfAOFXD1lAtctkMTUODg)); combined variable-rate model ([DAFx archive](https://dafx.de/paper-archive/details/KbFTgvcTMmHQ2bHZvOUekw)) |
| Spring reverb | Dispersion (chirps), multiple springs, transducer coloration | Convolution with measured IR (static) | Parametric dispersive allpass chains | Physical model of the spring | Spring and plate models covered in DAFX book ch. 12 ([contents](https://dafx.de/DAFX_Book_Page_2nd_edition/chapter12.html)) |
| Tape echo | Wow and flutter, head gap, saturation, bias | Modulated delay + saturation + EQ | Tape transport + signal path model | Magnetic hysteresis model | DAFX book ch. 12 covers tape-based echo ([contents](https://dafx.de/DAFX_Book_Page_2nd_edition/chapter12.html)) |
| Drive pedals | Gain-stage response vs knob, op-amp limits, diode/JFET clipping | Linear stage + static clipper, oversampled | Dynamic clipper solver + GBW + slew (project default) | Full nodal model | Project compendiums: MXR, BD-2, Magus Pro |

For pedals, the project has already settled the core method (Section 2): one diode-node solver reused across topologies, knob-dependent linear sections parameterised by time constants rather than frequency and Q, and 8× oversampling at maximum gain (MXR measurement: −92.7 dB alias metric at 8× vs −25 dB at 1× for a 1.23 kHz tone *(computed, project)*).

## 10. Numerical methods: discretisation, solvers, aliasing, oversampling

Three numerical decisions apply to every block: how continuous time becomes discrete, how implicit equations are solved, and how aliasing is controlled. Make them once per instrument and apply them consistently.

### 10.1 Discretisation

| Method | Accuracy | Stability | Cost | Use |
| --- | --- | --- | --- | --- |
| Forward Euler (explicit) | Poor at high cutoff; needs tuning polynomials | Can go unstable at high cutoff | Lowest | Legacy models (Huovilainen-style with corrections) |
| Backward Euler | First-order; damps resonance | Very stable | Low (implicit) | Stiff nodes where damping is acceptable |
| Trapezoidal / bilinear in TPT form | Second-order; frequency warping near Nyquist | A-stable; robust under cutoff modulation | Low (implicit) | Default for filters and networks |
| Explicit Runge–Kutta (RK4) | High per step for smooth signals | Conditional | Medium | VCV Rack's ladder integrates a distributed Padé-tanh ODE with RK4 *(sourced, DAFx26)* |
| Möbius-transform one-step family, conformal maps | Reduces trapezoidal warping and artefacts without oversampling | Method-dependent | Low | When oversampling is unaffordable (DAFx-21) |
| Exact (matrix exponential) for linear parts | Exact for LTI segments | Stable | Medium | Slowly varying linear networks |

### 10.2 Antialiasing

| Technique | What it fixes | Cost | Caveats |
| --- | --- | --- | --- |
| Oversampling (2–16×) with good decimation filters | Aliasing and stiff-node accuracy together | Linear in factor | Choose by measurement: MXR needed 8× at max gain, 2–4× at moderate gain *(computed, project)* |
| ADAA, memoryless (Parker, Zavalishin & Le Bivic, DAFx-16) | Aliasing of static waveshapers | Low | Adds half-sample delay; needs antiderivative |
| ADAA for stateful systems (Holters, DAFx-19) and arbitrary-order IIR ADAA (DAFx-21) | Aliasing inside filters with nonlinear state | Medium; tables for implicit nonlinearities | Can shift poles and zeros of the linear response |
| BLEP corrections | Oscillator discontinuities | Low | Discontinuities only; not for nonlinear filters |

Rule from the MXR measurement: apply ADAA to the *last* hard nonlinearity first, or it buys almost nothing.

### 10.3 Practicalities

- **Rates.** Control layer at host rate with sample-accurate events; voice core in one oversampled island; effects at their own rate (BBD clock, or host rate).
- **Sample-rate invariance.** Derive every coefficient from physical time constants so 44.1, 48 and 96 kHz hosts sound the same.
- **Parameterise by time constants.** Pole/zero pairs that merge with a knob are better parameterised by τ than by frequency and Q (MXR §7.3).
- **Denormals and silence.** Inject tiny noise or flush denormals; hardware never sits at exact zero anyway.
- **Newton budget.** Cap iterations, seed from the previous sample, and log non-convergence in debug builds.

## 11. Per-unit variation, drift, noise, quirks and faults

No two vintage units are identical, so "the" hardware is a distribution. Emulate one well-defined nominal unit, then express variation as seeded, bounded perturbations of physical parameters, never as random effects bolted onto the output.

| Source of variation | Physical cause | How to model | Bound it by |
| --- | --- | --- | --- |
| Component tolerance | ±1–20% resistors and capacitors *(approx., part-dependent)* | Seeded per-unit perturbation of each value | The parts list tolerance column |
| Semiconductor spread | I\_DSS, V\_P, β, I\_s, n spread within a bin | Perturb device parameters per unit; mismatch per pair | Datasheet min/max (BD-2: same-bin JFET k varies >10×) |
| Trimmer calibration | Service-manual trims (TB-303: A = 110 Hz, octave checks; VCF trimmer) *(sourced)* | Hidden trim parameters with the factory target as default | Service-note procedure and tolerance |
| Thermal drift | V\_T ∝ temperature in expo converters and ladders | Slow temperature state driving V\_T; warm-up curve *(approx.)* | Measured drift if available |
| Pitch drift and noise | Thermal noise, supply ripple, slow drift | Low-frequency filtered noise on CV; hum at mains frequency only if measured | Recordings of held notes |
| Voice-to-voice spread (polysynths) | Each voice built from separate parts | Independent seed per voice | Per-voice measurements |
| Supply and battery | Sag, capacitance multipliers, low battery | Supply voltage as a parameter; dynamic sag state | Regulator design (BD-2: ≈8 V working rail *(sourced)*) |
| Ageing and faults | Dried electrolytics, scratchy or worn pots, leaky caps, failing BBDs, oxidised switches | Optional "condition" presets, off by default | Documented service reports |
| Modifications | Devil Fish, Keeley-style pedal mods, recaps with modern parts | Separate labelled presets, never mixed into the stock model | The mod's own documentation |

**Variation hygiene.** Calibrate the nominal model first with variation off. Then fit variation ranges to multiple units if recordings of several exist. Keep the seed exposed so users can recall "their" unit.

**Contaminated references.** A recording made on a modified or badly calibrated unit will pull calibration away from stock. The TB-303 compendium found Devil Fish figures presented as stock in an AI-written reference; the same happens with online demo recordings.

## 12. Calibration from online reference sounds and other emulators

Calibrate a circuit-informed model by fitting a small set of physically meaningful hidden parameters (trims, tolerances, device constants, signal levels), stage by stage, to features that survive the unknowns in online audio. Never fit internal constants freely to a mixed recording, and never treat another emulator as ground truth.

### 12.1 Reference hierarchy

| Rank | Reference | Good for | Weaknesses | How to use it |
| --- | --- | --- | --- | --- |
| 1 | Measurements on a real unit (swept sines, steps, DC sweeps at known settings) | Everything; absolute levels | Needs hardware access | Primary calibration target |
| 2 | SPICE of the factory schematic | Linear response, device laws, critical gains, onset levels | Ideal parts; no tolerances or thermal drift (the DAFx26 reference had the same limit) | Structural truth; fit the topology before the unit |
| 3 | Isolated, dry hardware recordings with documented settings (reference sets, sample packs with patch notes) | Envelopes, timing, accent behaviour, spectra per note | Unknown converter chain; settings often approximate | Feature-level fitting (12.3) |
| 4 | Demo videos with visible panels | Knob-to-sound mapping over a sweep | Compressed audio; positions read by eye *(approx.)* | Coarse mapping of tapers and ranges |
| 5 | Other emulators, hardware clones | Controlled settings, unlimited renders, cross-checks of conventions | Their own discretisation, calibration and errors | Differential tests and conventions, never constants (12.5) |
| 6 | Finished tracks | Plausibility listening | Mixed, processed, settings unknown | Final listening only |

### 12.2 Unknowns in online audio and how to neutralise them

| Unknown | Effect | Mitigation |
| --- | --- | --- |
| Lossy codec (MP3/AAC) | Removes content above ≈16 kHz, smears transients, alters phase | Fit only below the codec band; use magnitude features, not waveforms |
| Unknown gain and normalisation | Shifts drive into every nonlinearity | Treat input level as a fitted parameter; fit drive-dependent shape, not absolute loudness |
| Unknown tuning and sample-rate conversion | Pitch offset, slight speed change | Estimate f0 per note and fit a global pitch/speed scale first |
| Unknown knob positions | Everything depends on them | Fit positions as nuisance parameters per clip, bounded by the panel range; share physical parameters across clips |
| Mastering EQ, compression, reverb, pedals | Changes spectra and envelopes | Prefer dry clips; fit a low-order "channel" EQ as a nuisance; reject clips with audible effects |
| Modified or miscalibrated unit | Biases the stock model | Check for known mod signatures (e.g. Devil Fish ranges) before using a clip |
| Timing and alignment | Breaks sample-wise comparison | Align by onset detection and DTW; compare frames, not samples |

### 12.3 Staged procedure

1. **Fix the structure first.** Linearise the model and check the critical feedback gain, cutoff label definition and small-signal response against SPICE or derived poles (the audit's first three findings were all here).
2. **Calibrate control laws.** Pitch scale and offset (1 V/oct), cutoff law versus knob, envelope time constants versus knob, using f0 tracking and per-note feature trajectories.
3. **Calibrate static nonlinearity.** Drive sweeps: harmonic levels H2–H9 normalised to H1 versus input level, at zero resonance, so feedback does not confound the attribution (DAFx26 method).
4. **Calibrate resonance and feedback.** Resonance-peak frequency and gain versus level (large-signal cutoff shift), self-oscillation onset and sustain, bass loss versus resonance.
5. **Calibrate dynamic and stateful behaviour.** Retriggers, consecutive accents, slides, gate-off tails, coupling-capacitor recovery.
6. **Fit variation last.** Spread of parameters across several units, if recordings of several exist.
7. **Hold out.** Validate on patterns, notes and settings not used for fitting; finish with blind ABX listening.

### 12.4 Features and loss functions

| Feature | Extract with | Loss | Robust to |
| --- | --- | --- | --- |
| Harmonic levels per note (H1–H10) | f0-synchronous analysis or Farina exponential swept sine where you control the input | RMSE of H2–H9 normalised to H1 (DAFx26 metric) | Gain, codec above 16 kHz |
| Filter trajectory | Spectral envelope sampled at harmonics, fitted to the model's response; peak frequency and gain | Error in cents and dB over time | Gain, small timing errors |
| Amplitude envelope | RMS in 2–5 ms frames *(approx.)* | Log-RMS error after onset alignment | Codec, EQ |
| Spectral shape | Multi-resolution STFT magnitude, log-mel | Spectral convergence + log magnitude | Phase changes |
| Timing | Onsets, gate lengths, slide durations | Absolute error in ms | Everything spectral |
| Self-oscillation | Residual RMS after excitation stops | Level ratio in dB | Gain if normalised to drive |

Avoid sample-wise time-domain losses unless input, settings and alignment are all known (rank 1–2 references only).

### 12.5 Optimisation options

| Option | Accuracy | Cost | Flexibility | Use when |
| --- | --- | --- | --- | --- |
| A. Manual or coordinate search over a few physical parameters | Good when parameters are separable | Engineer time | High insight | Few parameters; early stages 1–2 |
| B. Derivative-free global search (CMA-ES, Bayesian optimisation) over bounded physical parameters | Good; handles non-smooth solvers | Many model runs; parallelisable | Works with any existing model | Default for stages 2–5 |
| C. Differentiable white-box model trained by gradient on input–output audio | Learns approximate component values from raw audio of a real device ([Esqueda, Kuznetsov & Parker, DAFx-21](https://dafx.de/paper-archive/details/BuG_2piIUOi3oZvJGTn48g)) | Requires a differentiable implementation of the model | High once built | Rank 1–3 references with enough clean data |
| D. Grey-box residual: keep the circuit model, learn a small correction | Captures unmodelled circuitry | Training plus runtime cost | Risky outside training settings | Only after A–C plateau, and labelled as such |

**Bounds and priors.** Bound every parameter by its physical tolerance (Section 11). A fit that wants a 1 µF capacitor at 3 µF is telling you the topology is wrong, not that the part drifted. Watch for degenerate pairs (input level versus drive, cutoff offset versus capacitor value) and fix one member from another source.

### 12.6 Using other emulators and clones as references

- **Use them for conventions and differential tests.** Render identical patterns at identical settings in the reference and in your model; compare features, and investigate every disagreement against the schematic.
- **Never import constants.** The audit's Open303 lesson: its resonance polynomial compensates its own discretisation and self-oscillates in a trapezoidal solver *(computed, project)*.
- **Align conventions explicitly.** The DAFx26 benchmark needed per-model input scaling, resonance remapping and passband correction just to compare cores fairly; do the same before comparing.
- **Triangulate.** Trust a behaviour only when two independent emulators and one hardware recording agree, or the schematic predicts it.
- **Grade clones.** Schematic-based hardware clones are closer to rank 1 than software, but use modern parts (e.g. the V662A reissue for the BA662); record which parts differ.

## 13. Existing emulation landscape and quality tiers

Judge an existing emulation by which structural behaviours it reproduces, not by its marketing or its linear response. Only the open-source implementations below were checked against source or a published benchmark; commercial products need their own per-instrument assessment.

### 13.1 Quality tiers

| Tier | Structure | Typical result | How to recognise it |
| --- | --- | --- | --- |
| 0. Sample playback | Recorded notes | Exact per sample, static, no knob interaction | Identical repeated notes; no interaction between controls |
| 1. Generic VA | Band-limited oscillators, linear filter, output saturator | Right on held notes at low drive | Resonance does not shift or compress with level; no bass loss |
| 2. Structured nonlinear | Huovilainen-type cascades, oversampled | Lively, close to SPICE on ladders | Drive changes harmonics and peak frequency |
| 3. Circuit-informed ZDF | TPT core with distributed nonlinearity, current-domain CV, persistent state | Correct pattern-level behaviour when calibrated | Consecutive accents, retriggers and slides behave like hardware |
| 4. Component-level | DK / WDF / port-Hamiltonian from the schematic | Highest fidelity; limited by unknown values | Matches loading-dependent effects, coupling poles |
| 5. Learned | Black-box or grey-box neural models | Excellent inside training data | Artefacts at unseen settings or audio-rate modulation |

### 13.2 Open-source references checked

| Implementation | Structure (as documented) | Measured quality | Source |
| --- | --- | --- | --- |
| Csound moogladder | Huovilainen lineage, 1+4 tanh, 2× OS | Close to SPICE across harmonic metrics | DAFx26 benchmark |
| VCV Rack Fundamental VCF | Distributed Padé-tanh ladder ODE, RK4 | Close to SPICE; farthest of the distributed group | DAFx26 benchmark |
| JUCE LadderFilter | tanh at input and feedback only; four linear sections | Competitive linear response; large nonlinear errors; no sustained self-oscillation at the test setting | DAFx26 benchmark |
| Open303 TeeBeeFilter (TB\_303 mode) | Linear ladder ODE, k anchored at 17, explicit discretisation with compensating polynomial, resonance skew and gain compensation | Cutoff label equals resonance-peak frequency; no self-oscillation; internally consistent | Project audit (source read) |

The audit also found that Open303's calibration only works as a set: constants copied without the skew, gain compensation and cutoff fit broke the importing model *(computed, project)*.

### 13.3 What an instrument compendium's §7 must record

For each existing software or hardware emulation: its tier, which of the instrument's character-defining behaviours it reproduces (the instrument's own validation matrix is the checklist), any documented algorithm, its knob conventions, and whether claims come from the vendor or from an independent measurement. Hardware clones belong here too, with their substituted parts listed.

## 14. Faithful behaviour vs added features; myths corrected

Ship a faithful mode that does only what the hardware does, and put every convenience behind a clearly labelled switch whose default is off in that mode.

### 14.1 Faithful vs added

| Faithful (reproduce, never "fix") | Added feature (label, default off in faithful mode) |
| --- | --- |
| Level loss with resonance | Resonance gain compensation |
| Pitch drift, voice spread, tuning trims | Perfect tuning, drift amount control |
| Hardware knob ranges and tapers | Extended ranges, alternative curves |
| Control quantisation and scan-rate stepping | Smoothed high-resolution parameters |
| Monophony, note priority, retrigger rules | Polyphony, unison, legato modes |
| Accent as a step flag (TB-303 has no velocity loudness *(sourced)*) | Velocity-to-accent threshold mapping |
| No clean self-oscillation where the circuit has none | Self-oscillation option |
| Built-in effects as the hardware routes them | Extra effects, reordering |
| Aliasing of genuinely digital vintage oscillators | Antialiased variants |
| Stock circuit | Mod presets (Devil Fish, pedal mods), each labelled |

### 14.2 Myths corrected

| Myth | Correction | Evidence |
| --- | --- | --- |
| "Matching the frequency response means the filter is accurate." | Linear agreement within fractions of a dB coexisted with large nonlinear divergence | DAFx26 benchmark |
| "A tanh at the input and in the feedback captures a ladder." | Boundary saturation missed peak shift by hundreds of cents and lost self-oscillation | DAFx26 benchmark |
| "Any soft saturator will do." | atan and soft-clip substitutions shifted harmonic balance; JFET pairs have a finite hard knee | DAFx26; BD-2 |
| "Oversampling fixes nonlinear models." | It fixes aliasing and stiffness, not a wrong topology or a loop gain past critical | Audit; MXR |
| "ADAA on any nonlinearity reduces aliasing." | It helps only on the dominant (usually last) hard nonlinearity | MXR |
| "Copy the constants from a respected emulator." | Constants encode that emulator's discretisation and calibration | Audit (Open303) |
| "Resonance is Q." | It is a nonlinear feedback loop with its own filtering | TB-303; audit |
| "Add envelope depth to cutoff in Hz." | Control sums in current or voltage before an exponential converter | TB-303 Service Notes |
| "Analog warmth is noise and saturation on the output." | Character comes from where the nonlinearities and states sit inside the circuit | All four compendiums |
| "The TB-303 filter is simply 18 dB/oct." | Physically four-pole; uneven poles make it behave closer to 18 dB over part of the band | TB-303 compendium |
| "Published circuit analyses are reliable." | Arithmetic and topology errors were found and corrected against schematics | MXR; BD-2 |

## 15. Open questions and validation test matrix

### 15.1 Open questions

- How do harmonic-error metrics such as H2–H9 RMSE map to audibility? The DAFx26 author notes THD-style measures do not map reliably onto perceived distortion.
- Does "keep earlier ladder stages nonlinear" generalise to diode ladders and OTA cascades, or only to the buffered Moog ladder tested?
- How much do audio-rate cutoff modulation and FM expose differences between solver families? Not covered by the benchmark read.
- Can differentiable white-box calibration work from lossy online audio, or does it need clean measurements?
- Which polysynth filter chips (CEM, SSM, IR3109 class) have trustworthy datasheets or reissue datasheets usable for behavioural models?
- What is the right warm-up and drift model for each instrument? Few sources give measured data.

### 15.2 Validation matrix

Run the levels in order; a failure at one level invalidates everything above it.

| Level | Test | Pass criterion | Typical failure it catches |
| --- | --- | --- | --- |
| 1. Structure | Linearised poles and critical feedback gain vs derivation or SPICE | Within the derivation's precision | Wrong ODE, invented constants (audit findings 1–3) |
| 2. Small-signal | Swept sine at very low level, several cutoffs and resonances | Within ≈0.5 dB of reference below 10 kHz *(approx. target)* | Wrong corners, warping, cutoff label |
| 3. Control laws | Pitch per octave; cutoff and envelope times vs knob | Within trim tolerance | Wrong tapers, Hz-domain summing |
| 4. Static nonlinearity | Drive sweep at zero resonance; H2–H9 vs H1 | Harmonic RMSE comparable to the best distributed models | Boundary saturation, wrong saturator law |
| 5. Resonance | Peak frequency and gain vs level; bass loss vs resonance | Peak within ≈20 cents of reference *(approx., from DAFx26 range)* | Linear-Q resonance, missing feedback filtering |
| 6. Self-oscillation | Onset threshold and sustain level; or its absence | Matches hardware behaviour, including "none" | Loop gain past critical (audit) |
| 7. Aliasing | Sine sweeps at max drive; non-harmonic energy metric (MXR method) | Below −60 dB at the highest gain and pitch *(approx. target)* | Too little oversampling; ADAA on the wrong stage |
| 8. Modulation | Fast cutoff and resonance sweeps; audio-rate cutoff FM | No zipper, no instability | Non-TPT structures, unsmoothed controls |
| 9. State | Retriggers, consecutive accents, slides, rests, gate-off tails | Matches hardware sequences | Resets at note boundaries |
| 10. Sample-rate invariance | Render at 44.1, 48, 96, 192 kHz | Feature differences inaudible | Coefficients tied to sample rate |
| 11. Robustness | Extreme settings, silence, DC input, denormal tails | No NaN, no CPU spikes, no runaway | Solver non-convergence |
| 12. Performance | CPU per voice at target polyphony and oversampling | Within budget | Unbounded Newton iterations |
| 13. Reference match | Calibrated model vs held-out recordings (Section 12) | Feature losses at or below calibration-set level | Overfitting |
| 14. Listening | Blind ABX against hardware or best reference | Not reliably distinguishable, or differences documented | Everything the metrics missed |

## 16. Sources

**Project documents read in full or in the relevant sections** (their own source lists cover the Service Notes, Whittle, Stinchcombe, ElectroSmash, datasheets and pedal literature):

- TB-303 Hardware-Accurate Emulation Compendium (2026-09-18)
- TB303\_EMULATION\_REFERENCE.md (AI-written; treated as secondary, as in the TB-303 compendium)
- TB-303 Filter Audit (2026-09-19), including its reading of Open303's `TeeBeeFilter` source
- MXR Distortion + Hardware-Accurate Emulation Compendium
- Boss BD-2 Blues Driver Hardware-Accurate Emulation Compendium
- TC Electronic Magus Pro Emulation Compendium
- Pedal Emulation Research Queue

**Read this session (full text or substantial portions):**

- V. Zavalishin, *The Art of VA Filter Design*, rev. 2.1.0 (2018), chapters 1–3 and table of contents — [PDF](https://www.native-instruments.com/fileadmin/ni_media/downloads/pdf/VAFilterDesign_2.1.0.pdf)
- H. Oyama, "Quantifying Nonlinear Behavior in Digital Moog Ladder Filters: Cross-Implementation Comparison and Common-Core Ablation," DAFx26 — [PDF](https://www.dafx.de/paper-archive/2026/papers/DAFx26_paper_29.pdf)

**Located this session; abstract, listing or metadata read only** (verify details in the full text before relying on them):

- A. Huovilainen, "Non-Linear Digital Implementation of the Moog Ladder Filter," DAFx-04 — [abstract](https://dafx.de/paper-archive/details/UervgvkeeDC1a4sWDluM4Q)
- S. D'Angelo, *Virtual Analog Modeling of Nonlinear Musical Circuits*, Aalto doctoral thesis 2014 (WDF generalisation, Buchla LPG, generalised Moog ladder) — [record](https://aaltodoc.aalto.fi/items/08b88f1e-4cc4-4dee-a53f-8aa63c5c5da4)
- K. J. Werner, R. McClellan, "Moog Ladder Filter Generalizations Based on State Variable Filters," DAFx-20 — [abstract](https://dafx.de/paper-archive/details/tKYhnWc19H9rsRWpx9vyog)
- "Improving the Chamberlin Digital State Variable Filter" (reference list used for EMS VCS3 and D'Angelo citations) — [arXiv 2111.05592](https://arxiv.org/pdf/2111.05592)
- V. Välimäki, J. Pekonen, J. Nam, "Perceptually informed synthesis of bandlimited classical waveforms using integrated polynomial interpolation," JASA 2012 — [abstract](https://koasas.kaist.ac.kr/handle/10203/201388)
- J. Nam, V. Välimäki, J. S. Abel, J. O. Smith, "Efficient antialiasing oscillator algorithms using low-order fractional delay filters," IEEE TASLP 2010 — [abstract](https://ccrma.stanford.edu/papers/efficient-antialiasing-oscillator-algorithms-using-low-order-fractional-delay-filters)
- C. Raffel, J. Smith, "Practical Modeling of Bucket-Brigade Device Circuits," DAFx-10 — [abstract](https://dafx.de/paper-archive/details/JhVfAOFXD1lAtctkMTUODg)
- "A Combined Model for a Bucket Brigade Device and its Input and Output Filters," DAFx — [abstract](https://dafx.de/paper-archive/details/KbFTgvcTMmHQ2bHZvOUekw)
- F. Esqueda, B. Kuznetsov, J. D. Parker, "Differentiable White-Box Virtual Analog Modeling," DAFx-21 — [abstract](https://dafx.de/paper-archive/details/BuG_2piIUOi3oZvJGTn48g)
- DAFx-21 virtual-analog session listing (port-Hamiltonian Korg35 and Moog VCF, non-iterative schemes, arbitrary-order IIR ADAA, RNN exposure bias) — [listing](https://pub.mdw.ac.at/pubmdw/publication/b7503769-3fc2-4827-842e-e8df5a8cbaa6)
- DAFx archive entries for Möbius-transform discretisation and conformal maps — [archive page](https://dafx.de/paper-archive/search?p=137)
- *DAFX: Digital Audio Effects*, 2nd ed., chapter 12 contents (Moog ladder, WDF, spring, plate, tape echo) — [contents](https://dafx.de/DAFX_Book_Page_2nd_edition/chapter12.html)
- Aalto University virtual analog research page (publication list) — [page](https://www.aalto.fi/en/department-of-signal-processing-and-acoustics/virtual-analog-synthesis-and-audio-effects)

**Cited via the project compendiums, not re-read this session:** Holters, "Antiderivative Antialiasing for Stateful Systems" (DAFx-19); Parker, Zavalishin, Le Bivic, "Reducing the Aliasing of Nonlinear Waveshaping Using Continuous-Time Convolution" (DAFx-16); Werner et al., WDF diode-clipper and op-amp papers; Yeh, Abel, Smith, DAFx-06/07 and CMJ 2008; the Lambert-W approximation paper (DAFx-19). Links are in the MXR and BD-2 compendiums.

**Not retrieved; general knowledge to verify:** filter-topology attributions marked *(general knowledge, unverified)* in Section 6.1; DCO behaviour in Section 5.3; nodal DK method details (Holters and Zölzer); Boyle op-amp macromodel.
