# Lexicon oracle (reference only)

`tearwash_oracle` renders calibration stimuli through the original Lexicon 224XL V8.21 firmware,
emulated by [BlueBox](https://github.com/jimbattin/bluebox), so `tearwash_calib` can compare our
reverb engines against it (`docs/tearwash/04_VALIDATION.md`).

```
tools/oracle/setup_bluebox.sh [SET_FILE] [OUT_DIR]   # default: calib_xl.txt -> build/oracle/xl
build/tearwash_calib build/oracle/xl --report docs/tearwash/reports/NAME.md --tsv build/NAME.tsv
```

The script clones BlueBox at a pinned commit into `build/ext/bluebox`, unpacks the ROM images
from `training/lex/Lexicon 224 and 224X Firmware.zip` into `build/ext/roms/`, builds this
directory against BlueBox's `emu/src` and renders the set (about 1 s per case).

## Supplying the ROMs

The ROM images are proprietary and are **not in the repository** (they were removed from the
history on 2026-10-10). The user keeps them and supplies them for a comparison session:

1. Upload `Lexicon 224 and 224X Firmware.zip` (the archive with the `224XL V8.21`, `224X V8.1`
   and `224 V4.4` folders) to the session.
2. Put it at `training/lex/Lexicon 224 and 224X Firmware.zip`. `training/lex/` is git-ignored, so
   it cannot be committed by accident; check with `git status` that it does not show up.
3. Run `tools/oracle/setup_bluebox.sh` (it unpacks into `build/ext/roms/`), then
   `tools/oracle/capture_all.sh` if bit-exact captures for `tearwash_core_test` are needed
   (otherwise those checks are skipped).
4. When done, the zip and `build/` may be deleted; nothing derived from the ROM bytes other than
   our own measured constants (tagged [R]) and reports goes into commits.

Rules: BlueBox has no licence and the ROMs are proprietary. Both stay under `build/`
(git-ignored); nothing from either is committed, embedded in a plugin or loaded at runtime.
The renders are reference measurements; commit metrics and reports, not audio.

Set file format (`calib_xl.txt`): `ID PROGRAM STIMULUS SECONDS [P.S=HH ...] [opt=MASK:BITS]`,
with `_` for spaces in program names, LARC page/slider and a hex slider code, and option bits
01 DYN DECAY, 40 MODE ENH, 80 DECAY OPT forced at load. Stimuli are defined in
`src/tearwash/analysis/Stimuli.hpp`. `tearwash_oracle --rom-dir DIR --list` prints the programs.
