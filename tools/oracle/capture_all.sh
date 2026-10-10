#!/usr/bin/env bash
# Regenerate the bit-exact captures that tearwash_core_test (W2b) compares the native networks
# against: the original program's state, input and DAC output, captured by tearwash_oracle.
# Needs tools/oracle/setup_bluebox.sh to have run (ROMs supplied, see README.md). Output stays
# under build/ (git-ignored).
#
# usage: tools/oracle/capture_all.sh [OUT_DIR]     # default build/capture
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:-$ROOT/build/capture}"
TOOL="$ROOT/build/oracle-tool/tearwash_oracle"
ROMS="$ROOT/build/ext/roms/224XL_v8_21"
[ -x "$TOOL" ] && [ -d "$ROMS" ] || { echo "run tools/oracle/setup_bluebox.sh first (ROMs: tools/oracle/README.md)" >&2; exit 1; }
mkdir -p "$OUT"
cap() { local id=$1 prog=$2 frames=$3; shift 3; "$TOOL" --rom-dir "$ROMS" --capture "$prog" "$frames" "$OUT/$id.bin" "$@" | tail -1; }

# CONCERT HALL (01): factory, page 1, page 3, pre-echo pages, extremes, SIZE (rom: patches the
# program's SIZE byte before loading).
cap 01     01 40000
cap 01_a   01 30000 1.1=C0 1.2=40 1.3=60 1.4=A0 1.5=AA 1.6=30
cap 01_b   01 30000 3.4=E0 3.5=80 3.6=B0 1.2=E8 1.5=10
cap 01_c   01 30000 4.1=40 4.2=80 4.3=20 4.4=FE 5.1=30 5.2=50 5.3=08 5.4=90 5.5=10 5.6=60 6.1=40 6.3=C0 6.6=80
cap 01_d   01 30000 1.1=00 1.2=FF 1.3=FF 1.4=FE 3.4=FF 3.5=FF 3.6=FF 1.6=C8
cap 01_s56 01 30000 rom:F030=56 1.6=FF 5.1=80 5.5=40 4.1=80
cap 01_s20 01 30000 rom:F030=20 1.6=C0
# ROOM (04), SMALL PLATE (03): factory.
cap 04     04 30000
cap 03     03 30000
# PLATE (02).
cap 02     02 30000
cap 02_a   02 30000 1.1=C0 1.2=30 1.3=60 1.4=A0 1.5=AA 1.6=30
cap 02_b   02 30000 3.4=E0 3.5=40 3.6=B0 1.2=E8 1.5=FE
cap 02_c   02 30000 4.1=40 4.2=80 4.3=20 4.4=FE 4.5=10 4.6=C0 5.1=30 5.2=50 5.3=08 5.4=90 5.5=10 5.6=60 6.1=40 6.3=C0 6.6=80 1.6=F0
cap 02_s74 02 30000 rom:F468=74 1.6=FF 5.3=60 4.3=80
cap 02_s00 02 30000 rom:F468=00 1.6=A0
# CHAMBER (08).
cap 08     08 30000
cap 08_a   08 30000 1.1=C0 1.2=30 1.3=60 1.4=A0 1.5=AA 1.6=90
cap 08_b   08 30000 3.4=E0 3.5=FF 1.2=E8 1.5=FE 1.1=10
cap 08_c   08 30000 1.6=FF 1.3=FF 1.4=FF 3.4=40 3.5=10 1.5=55
