# Development guidelines (humans and AI agents)

Tannhäuser is a Yamaha CS-80 emulation, a C++17 `.clap` plugin with no third-party DSP or
GUI libraries (same constraints as Acidus/Gritbaal).

## Where things are

- Start with `docs/PROJECT_PLAN.md`: work packages, open issues (WP tables), open hardware
  questions (P-1…), decision log. Keep it current: update the status of every issue you
  touch and add a line to the progress log.
- `docs/spec/` is the implementation specification. Code and spec must agree; when you
  change a law or a constant, change the spec in the same commit, with its evidence tag
  ([S] sourced, [I] inferred, [D] default, [A] added feature).
- `docs/reference/` holds the original research; read it only to re-check evidence.
- Tearwash 225 (the Lexicon 224-family reverb, own plugin and Tannhäuser's reverb): spec in
  `docs/tearwash/`, plan in `docs/tearwash/PLAN.md`. ROM images in `training/lex/` and the
  BlueBox emulator are reference-only: oracle and analysis tools under `build/`, never committed,
  never in a plugin (`docs/tearwash/01_ARCHITECTURE.md` §7). Oracle: `tools/oracle/`.
- Parameters: `src/core/Params.cpp` is the single source of truth, mirrored in
  `docs/spec/04_PARAMETERS.md`. Never renumber ids or rename keys (state and presets use
  them); only append.
- Presets: edit `tools/preset_library.py`, then run `python3 tools/gen_presets.py` (it
  validates keys and regenerates `src/presets/PresetData.cpp`; never edit that file).
  Then rebuild, run `build/tannhauser_loudness --fit tools/preset_gains.json`, regenerate and
  rebuild again so the new preset is loudness-matched (test T16).

## Rules

1. Faithful hardware behaviour by default; anything not on the CS-80 is an added feature,
   labelled [A] in the spec and the plan.
2. Sum control signals in the circuit's own domain (volts), then apply the law
   (general compendium §2). Derive coefficients from times/frequencies, never per sample rate.
3. Nothing allocates, locks (except `try_lock`) or throws on the audio thread.
4. After DSP changes run `build/tannhauser_dsp_test`; after GUI/plugin changes run
   `build/tannhauser_gui_test` (pass a path to get a `.ppm` snapshot to look at).
   Every preset must pass T12 (finite, peak < 1 for an 8-note chord).
5. GUI drawing is procedural (shaded at 2× and box-filtered); keep each control's drawing
   inside its bounds (`GuiWindow.cpp` `margins`) so incremental redraws stay clean.
