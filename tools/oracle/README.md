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

Rules: BlueBox has no licence and the ROMs are proprietary. Both stay under `build/`
(git-ignored); nothing from either is committed, embedded in a plugin or loaded at runtime.
The renders are reference measurements; commit metrics and reports, not audio.

Set file format (`calib_xl.txt`): `ID PROGRAM STIMULUS SECONDS [P.S=HH ...] [opt=MASK:BITS]`,
with `_` for spaces in program names, LARC page/slider and a hex slider code, and option bits
01 DYN DECAY, 40 MODE ENH, 80 DECAY OPT forced at load. Stimuli are defined in
`src/tearwash/analysis/Stimuli.hpp`. `tearwash_oracle --rom-dir DIR --list` prints the programs.
