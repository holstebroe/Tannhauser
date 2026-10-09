#!/usr/bin/env python3
"""Test the time-row polarity of the decoded factory tones (plan issue P-1).

No recording of a factory tone is available, so this scores each hypothesis
against what each named instrument tone plausibly does: a harpsichord has a
fast attack and a short release, strings a slow attack and a long release,
and so on. The expected ranges below are this project's judgement [D], not
measurements; widen them rather than tune them to favour an answer.

Hypotheses for every time row (VCF A/D/R rows 13-15, VCA A/D/R rows 18/19/21):
  inv  higher voltage = shorter time   (pos = 1 - V/10, compendium §14.6)
  dir  higher voltage = longer time    (pos = V/10)
Each row is scored separately as well, to see whether a mixed polarity (one
row wired the other way) explains the data better than either global one.

    python3 tools/analyze_factory_polarity.py [--verbose]
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

ATTACK_ROWS = {13, 18}
TIME_ROWS = {13: "VCF A", 14: "VCF D", 15: "VCF R", 18: "VCA A", 19: "VCA D", 21: "VCA R"}


def seconds(row, v, hyp):
    pos = 1.0 - v / 10.0 if hyp == "inv" else v / 10.0
    return (0.001 if row in ATTACK_ROWS else 0.010) * 1000.0 ** pos


# Plausible time ranges in seconds per tone family [D]. Missing rows are not scored.
INF = 1e9
FAMILIES = {
    "String":      {18: (0.030, 1.5), 21: (0.25, 5.0), 13: (0.030, 2.0)},
    "Brass":       {18: (0.012, 0.25), 21: (0.04, 1.0), 13: (0.030, 0.8), 14: (0.08, 3.0)},
    "Flute":       {18: (0.012, 0.25), 21: (0.04, 1.0), 19: (0.15, 4.0)},
    "Electric":    {18: (0.0, 0.015), 19: (0.4, 6.0), 21: (0.03, 1.0), 14: (0.08, 4.0)},
    "Clavichord":  {18: (0.0, 0.010), 19: (0.08, 2.0), 21: (0.0, 0.25), 14: (0.04, 2.0)},
    "Harpsichord": {18: (0.0, 0.010), 19: (0.25, 5.0), 21: (0.0, 0.35)},
    "Organ":       {18: (0.0, 0.040), 21: (0.0, 0.25)},
    "Guitar":      {18: (0.0, 0.015), 19: (0.3, 8.0), 21: (0.02, 1.5)},
    "Funky":       {18: (0.0, 0.060), 19: (0.06, 2.0), 21: (0.0, 0.5), 14: (0.04, 2.0)},
    "Bass":        {18: (0.0, 0.040), 19: (0.08, 4.0), 21: (0.0, 0.5), 14: (0.04, 3.0)},
}


def family(name):
    for f in FAMILIES:
        if name.startswith(f):
            return f
    return None


def relevant(row, volts):
    """Rows whose time is audible for this tone."""
    s = volts.get("20", 0.0) / 10.0       # VCA sustain level
    il = volts.get("11", 0.0) / 10.0
    al = volts.get("12", 0.0) / 10.0
    if row == 19:
        return s < 0.5                     # decay only matters when it falls far
    if row == 13:
        return al > 0.15 or il > 0.15
    if row == 14:
        return al > 0.15
    if row == 15:
        return il > 0.15
    return True


def score(volts, fam, hyp):
    """(plausible, scored) for one bus's data judged as tone family `fam`."""
    ok = n = 0
    for row, (lo, hi) in FAMILIES[fam].items():
        if not relevant(row, volts):
            continue
        n += 1
        ok += lo <= seconds(row, float(volts.get(str(row), 0.0)), "inv" if hyp == "inv" else "dir") <= hi
    return ok, n


def best_assignment(data):
    """Bus-to-button order test (plan P-9): which assignment of each channel's
    11 bus data sets to its 11 button names is most plausible (inv polarity)?
    Exact bitmask DP over permutations."""
    for ch, tones in data["channels"].items():
        names = list(tones.keys())             # printed (assumed) order = bus order
        fams = [family(n) for n in names]
        k = len(names)
        frac = [[0.0] * k for _ in range(k)]   # frac[bus][button]
        for i, n in enumerate(names):
            for j in range(k):
                ok, cnt = score(tones[n]["volts"], fams[j], "inv")
                frac[i][j] = ok / cnt if cnt else 0.0
        best = {0: (0.0, [])}
        for i in range(k):
            nxt = {}
            for mask, (sc, assign) in best.items():
                for j in range(k):
                    if mask & (1 << j):
                        continue
                    cand = (sc + frac[i][j], assign + [j])
                    m2 = mask | (1 << j)
                    if m2 not in nxt or cand[0] > nxt[m2][0]:
                        nxt[m2] = cand
            best = nxt
        sc, assign = max(best.values(), key=lambda t: t[0])
        printed = sum(frac[i][i] for i in range(k))
        print(f"Channel {ch}: printed order fits {printed:.2f}/{k}, best order fits {sc:.2f}/{k}")
        for i, j in enumerate(assign):
            if i != j and frac[i][j] > frac[i][i] + 0.25:   # ignore ties
                print(f"  bus of '{names[i]}' fits '{names[j]}' better ({frac[i][j]:.2f} vs {frac[i][i]:.2f})")


def main():
    verbose = "--verbose" in sys.argv
    data = json.loads((ROOT / "docs/reference/cs80_presets.json").read_text())
    total = {"inv": 0, "dir": 0}
    per_row = {r: {"inv": 0, "dir": 0, "n": 0} for r in TIME_ROWS}
    failures = {"inv": [], "dir": []}
    n = 0
    for ch, tones in data["channels"].items():
        for name, t in tones.items():
            fam = family(name)
            if not fam:
                continue
            volts = t["volts"]
            for row, (lo, hi) in FAMILIES[fam].items():
                if not relevant(row, volts):
                    continue
                v = float(volts.get(str(row), 0.0))
                n += 1
                per_row[row]["n"] += 1
                line = f"  {name:14s} {TIME_ROWS[row]}  {v:4.1f} V"
                for hyp in ("inv", "dir"):
                    sec = seconds(row, v, hyp)
                    ok = lo <= sec <= hi
                    total[hyp] += ok
                    per_row[row][hyp] += ok
                    line += f"   {hyp} {sec * 1000:8.1f} ms {'ok ' if ok else 'BAD'}"
                    if not ok:
                        failures[hyp].append(f"{name} {TIME_ROWS[row]} ({sec * 1000:.0f} ms, want {lo * 1000:.0f}-{hi * 1000:.0f} ms)")
                if verbose:
                    print(line)
    print(f"Scored {n} audible time settings over 22 factory tones.\n")
    print(f"Global 'higher V = shorter' (inv): {total['inv']}/{n} plausible")
    print(f"Global 'higher V = longer'  (dir): {total['dir']}/{n} plausible\n")
    print("Per row (inv / dir plausible of n):")
    best_mixed = 0
    for row, r in per_row.items():
        if r["n"] == 0:
            continue
        best_mixed += max(r["inv"], r["dir"])
        print(f"  row {row:2d} {TIME_ROWS[row]}: {r['inv']:2d} / {r['dir']:2d} of {r['n']}")
    print(f"\nBest mixed polarity (each row its own winner): {best_mixed}/{n}")
    print("\nImplausible under inv:")
    for f in failures["inv"]:
        print("  " + f)
    print()
    best_assignment(data)
    if verbose:
        print("\nImplausible under dir:")
        for f in failures["dir"]:
            print("  " + f)


if __name__ == "__main__":
    main()
