#!/usr/bin/env python3
"""Compare sim.exe species G-values with Fig. 3 of Boscolo et al., IJMS 2020, 21, 424.

Usage: compare_boscolo_fig3.py <dump-dir> [<dump-dir> ...] [--out <png>]

Each dump dir holds Species_nt_species.csv and Manifest.json (see the sim-output
skill). G = sumG / nEvent; pO2 % = O2 molarity / 0.0013 M * 100 (0 if no O2),
snapped to the nearest level of analysis/reference/boscolo2020_fig3.csv.
Writes a 2x2 figure, prints a PASS/FAIL/SKIP table, exits 1 if any check FAILs.
"""
import argparse
import json
import re
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

HENRY_O2_M = 0.0013                      # M per 100 % pO2
REF_CSV = Path(__file__).parent / "reference" / "boscolo2020_fig3.csv"
SPECIES = ["e_aq", "H", "O2m", "HO2"]
COLORS = {0: "#0000ff", 0.5: "#7f00ff", 5: "#ff00aa", 10: "#ff0055", 21: "#ff0000"}


def species_key(name):
    """Map '<name>^<charge>' (with a possibly mis-decoded degree sign) to e_aq/H/O2m/HO2."""
    base, _, charge = name.partition("^")
    base = re.sub(r"[^A-Za-z0-9_]", "", base)
    if base == "e_aq":
        return "e_aq"
    if base == "H" and charge == "0":
        return "H"
    if base == "O_2" and charge == "-1":
        return "O2m"
    if base == "HO_2" and charge == "0":
        return "HO2"
    return None


def load_dump(path, levels):
    """Return (pO2 level, {species: (time_s array, G array)}) for one dump dir."""
    manifest = json.loads((path / "Manifest.json").read_text(encoding="utf-8"))
    o2 = sum(s["molarity_M"] for s in manifest.get("scavengers", []) if s["species"] == "O2")
    pct = o2 / HENRY_O2_M * 100.0
    level = min(levels, key=lambda lv: abs(lv - pct))
    df = pd.read_csv(path / "Species_nt_species.csv", comment="#", header=None,
                     names=["id", "number", "nEvent", "name", "time_ns", "sumG", "sumG2"],
                     encoding="utf-8", encoding_errors="replace")
    df["key"] = df["name"].map(species_key)
    df["G"] = df["sumG"] / df["nEvent"]
    curves = {}
    for key, grp in df.dropna(subset=["key"]).groupby("key"):
        grp = grp[grp["time_ns"] > 0].sort_values("time_ns")
        curves[key] = (grp["time_ns"].to_numpy() * 1e-9, grp["G"].to_numpy())
    return level, curves


def g_at(curve, t):
    """G at the output time nearest to t (log distance); None if none within 10 %."""
    times, g = curve
    i = int(np.argmin(np.abs(np.log(times / t))))
    return float(g[i]) if abs(np.log(times[i] / t)) < 0.1 else None


def g_before(curve, t):
    """(time, G) at the latest output time <= t, or None."""
    times, g = curve
    idx = np.nonzero(times <= t)[0]
    return (float(times[idx[-1]]), float(g[idx[-1]])) if len(idx) else None


def run_checks(ours):
    results = []

    def add(name, status, detail=""):
        results.append((name, status, detail))

    def need(level, sp):
        return ours.get(level, {}).get(sp)

    c = need(0, "H")
    if c is None:
        add("H(0%) non-increasing", "SKIP", "no pO2=0 dump")
    else:
        t, g = c
        m = t >= 1e-12 * 0.999
        t, g = t[m], g[m]
        bad = [(t[i], g[i], g[i + 1]) for i in range(len(g) - 1) if g[i + 1] > g[i] + 0.02]
        add("H(0%) non-increasing", "FAIL" if bad else "PASS",
            f"{len(bad)} rise(s) > 0.02" + (f", first at {bad[0][0]:.3g} s ({bad[0][1]:.3f} -> {bad[0][2]:.3f})" if bad else ""))

    for sp in ("O2m", "HO2"):
        name = f"{sp}(21%) saturated"
        c = need(21, sp)
        g1 = g_at(c, 1e-6) if c else None
        prev = g_before(c, 0.55e-6) if c else None   # 0.5 us, or the last output time before it
        if g1 is None or prev is None or g1 == 0:
            add(name, "SKIP", "no pO2=21 data at 1 us / before 0.5 us")
        else:
            d = abs(g1 - prev[1]) / g1
            add(name, "PASS" if d < 0.05 else "FAIL",
                f"G({prev[0]:.3g}s)={prev[1]:.3f} G(1us)={g1:.3f} rel.change={d:.3f}")

    c = need(0, "HO2")
    g1 = g_at(c, 1e-6) if c else None
    if g1 is None:
        add("HO2(0%) ~ 0", "SKIP", "no pO2=0 data at 1 us")
    else:
        add("HO2(0%) ~ 0", "PASS" if g1 < 0.05 else "FAIL", f"G(1us)={g1:.3f}")

    for sp, level, ref in (("O2m", 21, 2.24), ("HO2", 21, 0.66), ("e_aq", 0, 2.25), ("H", 0, 0.55)):
        name = f"{sp}({level}%) +-10%"
        c = need(level, sp)
        g1 = g_at(c, 1e-6) if c else None
        if g1 is None:
            add(name, "SKIP", f"no pO2={level} data at 1 us")
        else:
            rel = (g1 - ref) / ref
            add(name, "PASS" if abs(rel) <= 0.10 else "FAIL", f"G(1us)={g1:.3f} vs {ref} ({rel:+.1%})")
    return results


def plot(ref, ours, out):
    fig, axes = plt.subplots(2, 2, figsize=(11, 8), sharex=True)
    for ax, sp in zip(axes.flat, SPECIES):
        for level in sorted(COLORS):
            r = ref[(ref.species == sp) & (ref.pO2_pct == level)]
            ax.plot(r.time_s, r.G, "o", ms=4, color=COLORS[level], mfc="none")
            if sp in ours.get(level, {}):
                t, g = ours[level][sp]
                ax.plot(t, g, "-", color=COLORS[level], label=f"pO2={level:g}%")
        ax.set_xscale("log")
        ax.set_title(sp)
        ax.set_ylabel("G (molecules/100 eV)")
        ax.grid(alpha=0.3)
    for ax in axes[1]:
        ax.set_xlabel("time (s)")
    axes[0, 0].legend(title="solid: sim, o: Boscolo 2020 Fig. 3", fontsize=8)
    fig.tight_layout()
    fig.savefig(out, dpi=130)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("dirs", nargs="+", type=Path, help="dump directories")
    ap.add_argument("--out", type=Path, default=Path("boscolo_fig3_comparison.png"))
    args = ap.parse_args()

    ref = pd.read_csv(REF_CSV, comment="#")
    levels = sorted(ref.pO2_pct.unique())
    ours = {}
    for d in args.dirs:
        level, curves = load_dump(d, levels)
        print(f"{d}: pO2 level {level:g}% ({len(curves)} species curves)")
        ours[level] = curves
    plot(ref, ours, args.out)
    print(f"figure: {args.out}\n")

    results = run_checks(ours)
    w = max(len(r[0]) for r in results)
    for name, status, detail in results:
        print(f"{status:<5} {name:<{w}}  {detail}")
    return 1 if any(r[1] == "FAIL" for r in results) else 0


if __name__ == "__main__":
    sys.exit(main())
