# analysis

## compare_boscolo_fig3.py

Compares `sim.exe` species G-values with Fig. 3 of Boscolo et al., Int. J. Mol. Sci. 2020, 21, 424.

```bash
python analysis/compare_boscolo_fig3.py <dump-dir> [<dump-dir> ...] [--out fig.png]
```

Each dump dir is the output of one run (`Species_nt_species.csv` + `Manifest.json`). Its pO2 is the O2 scavenger molarity / 0.0013 M (0 % if no O2), snapped to the nearest reference level (0, 0.5, 5, 10, 21 %). Needs Python 3 with numpy, pandas, matplotlib. Reference data: `reference/boscolo2020_fig3.csv` (hand/vector-digitized, about +-0.02 G).

Output: a 2x2 PNG (e_aq, H, O2m, HO2; ours solid, reference as markers) and a PASS/FAIL/SKIP table. Exit code 1 if any check FAILs. A check whose pO2 level was not given is SKIP.

| Check | Meaning |
|---|---|
| `H(0%) non-increasing` | in anoxia H only decays: G(t_next) <= G(t) + 0.02 for successive output times >= 1 ps |
| `O2m(21%)` / `HO2(21%) saturated` | e_aq and H are consumed, so the yield has plateaued: relative change between 1 us and 0.5 us (or the last output time before it) < 5 % |
| `HO2(0%) ~ 0` | no O2 means no HO2: G(1 us) < 0.05 |
| `O2m(21%)`, `HO2(21%)`, `e_aq(0%)`, `H(0%)` `+-10%` | G at 1 us within 10 % of 2.24, 0.66, 2.25, 0.55 (paper text / Fig. 3) |
