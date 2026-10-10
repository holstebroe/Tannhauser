#!/usr/bin/env bash
# Fetch BlueBox (pinned), install the user's 224XL V8.21 ROM images from training/lex into it,
# build tearwash_oracle and render the calibration set (docs/tearwash/04_VALIDATION.md §1).
# Everything lands under build/ (git-ignored): nothing from BlueBox or the ROMs is committed.
#
# usage: tools/oracle/setup_bluebox.sh [SET_FILE] [OUT_DIR]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BB_COMMIT=b6531fe00a4e07d0a8ab5dbab886d6714e7390eb     # BlueBox main, 2026-10-03
BB="$ROOT/build/ext/bluebox"
ROMZIP="$ROOT/training/lex/Lexicon 224 and 224X Firmware.zip"
SET="${1:-$ROOT/tools/oracle/calib_xl.txt}"
OUT="${2:-$ROOT/build/oracle/xl}"

if [ ! -d "$BB/.git" ]; then
    mkdir -p "$ROOT/build/ext"
    git clone https://github.com/jimbattin/bluebox.git "$BB"
fi
git -C "$BB" fetch -q origin "$BB_COMMIT" 2>/dev/null || true
git -C "$BB" checkout -q "$BB_COMMIT"

ROMS="$ROOT/build/ext/roms"
if [ ! -f "$ROMS/224XL_v8_21/NVS8 2732.BIN" ]; then
    mkdir -p "$ROMS"
    unzip -qo "$ROMZIP" -d "$ROMS/raw" -x "__MACOSX/*" "*.DS_Store"
    for d in "224XL v8_21:224XL_v8_21" "224XL v8_1A:224XL_v8_1A" "224X v8_1:224X_v8_1" "224 v4_4:224_v4_4"; do
        mkdir -p "$ROMS/${d#*:}"
        cp "$ROMS/raw/Lexicon 224 and 224X Firmware/${d%%:*}/"*.BIN "$ROMS/${d#*:}/"
    done
fi

cmake -S "$ROOT/tools/oracle" -B "$ROOT/build/oracle-tool" -DBLUEBOX_DIR="$BB" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$ROOT/build/oracle-tool" -j >/dev/null
mkdir -p "$OUT"
"$ROOT/build/oracle-tool/tearwash_oracle" --rom-dir "$ROMS/224XL_v8_21" --set "$SET" --out "$OUT"
echo "oracle renders in $OUT"
