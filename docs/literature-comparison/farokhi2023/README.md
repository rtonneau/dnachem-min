# Farokhi2023 G-value comparison

Time-dependent G-values from dnachem-min's `Farokhi2023` chemistry, compared against
Farokhi et al. 2023 ("Effects of the Oxygen depletion in FLASH irradiation investigated
through Geant4-DNA toolkit", Radiat. Phys. Chem. 212, 111184), with a statistical-adequacy
check on the underlying Monte Carlo runs.

## Data

Production runs (not the smaller smoke runs used during development), 8 threads, IRT_syn
particle-based stage (no mesoscopic hand-over), `/scheduler/endTime 1 us`, 61 log-spaced
recording times (10 per decade, 1 ps to ~1 us):

| Particle | Energy | Box | O2 levels | Events/run |
|---|---|---|---|---|
| proton | 90 MeV | 10x10x10 um^3 (`/chem/env/halfBox 5 um`) | 0, 3, 7, 21 % (0, 31.1, 72.7, 218 uM) | 16 |
| electron | 500 keV | 500 um default + `PrimaryKiller eLossMin 5 keV` | 0, 2.5, 5, 21 % (0, 26.0, 51.9, 218 uM) | 56 |

Run on 2026-10-09, output at `<G4_DATA_OUTPUT>/261009_Farokhi2023_{proton,electron}/o2_*`
(machine-specific directory, not part of this repo; `plot_gvalues.py` reads it via the
`G4_DATA_OUTPUT` environment variable). O2 percentages are the paper's own Henry's-law
conversion at 37 C, not dnachem-min's `%` scavenger shortcut (see the macros' header comments
for the conversion).

## Regenerating

```
python plot_gvalues.py
```

Reads `Species_nt_species.csv` directly (no re-running of the simulation). Writes 5 files
next to itself: `farokhi2023_{proton,electron}_gvalues.png` (G(t) curves, shaded = +-1 SEM,
overlaid with hand-digitized reference points from the paper's Fig. 2/Fig. 5),
`farokhi2023_{proton,electron}_relerr.png` (relative SEM vs. time -- the statistics check),
and `statistics_summary.md` (the same check as a table).

## Statistics: is there enough?

Mean and standard error of the mean (SEM) come from each run's own `sumG`/`sumG2`
ntuple columns (sum and sum-of-squares of each *event's* own G-value,
`n_mol / eventEdep_eV * 100`), via the standard Bessel-corrected estimator:

```
mean = sumG / n
sample variance = (sumG2 - n * mean^2) / (n - 1)
SEM = sqrt(sample variance / n)
```

See `statistics_summary.md` for the full table (max relative SEM and fraction of the 61
recorded times above a 25 % relative-SEM threshold, per species/O2 level). Short version:

- **OH, H3Op, H2, H2O2** (the dominant, directly-produced species): relative SEM stays under
  ~16 % everywhere, for both particles and all O2 levels. These curves are statistically solid
  at the current event counts.
- **e_aq, H**: relative SEM stays under ~10 % for most of the 1 ps-1 us range, both particles.
  It grows late in time *only* where the mean itself is collapsing toward zero under strong O2
  scavenging (21 % O2, t > ~3e-7 s for the proton run in particular, where it reaches ~20-40 %)
  -- an artifact of dividing by a small mean, not a sign that the absolute counts are
  unreliable.
- **O2m, HO2** (the minor oxygen-scavenging products): this is where statistics are thin. Both
  species start at G = 0 and only begin forming once enough radiolytic species have diffused
  far enough to meet the dissolved O2 background, so most events still record exactly 0
  molecules through the early part of the window -- the SEM/mean ratio is at or near 100 % until
  the population builds up. For the **proton** run (16 events), this stays above the 25 %
  threshold for roughly half of the time points from onset to ~1 us at every non-zero O2 level
  (e.g. O2m at 21 % O2: 25/53 points over threshold). For the **electron** run (56 events), the
  same species clear the threshold earlier and more often stay below it afterward (e.g. O2m at
  21 % O2: 26/61 points over threshold, concentrated at the very earliest onset times) -- more
  than 3x the proton event count visibly tightens this, but does not eliminate it.
- At 0 % O2, O2m and HO2 are identically zero throughout (no scavenger reaction to fire),
  so no SEM is defined there (correctly reported as "n/a" rather than 0 %).

**Practical read:** the main-species curves (OH, H3Op, H, e_aq, H2, H2O2) are trustworthy for
shape/level comparison against the paper at these event counts. The O2m/HO2 curves' *overall
trend* (ordering by O2 level, onset time, late-time magnitude) is almost certainly right -- it
matches the paper's own curves and the proton/electron difference goes the expected direction
-- but the *exact shape* of their early rise is not well constrained at 16 events (proton) and
only partially better at 56 events (electron). Reproducing the paper's O2m/HO2 curves more
precisely would need substantially more events (order 100-300, based on how slowly the proton
relative-SEM table improves between the two event counts already run), concentrated on the
higher O2 levels where these species are most populated.

## Caveats (indicative only, not a reproduction)

- dnachem-min models a single homogeneous water box (10x10x10 um^3 for the proton runs, 500 um
  default for the electron runs), not the paper's nested sensitive-volume geometry.
- dnachem-min's `PhysicsList` uses `G4EmDNAPhysics_option2`; the paper does not state which
  `G4EmDNAPhysics` option (if any beyond "Geant4-DNA") it used.
- The reference points were read by eye off rendered pages of
  `docs/literature/Farokhi2023_Effects_of_the_Oxygen_depletion_in_FLASH_irradiation_investigated_through_geant4_DNA_toolkit.pdf`,
  at a handful of time points per curve -- approximate, hand-digitized values, not extracted
  with a digitizing tool, carrying a visible reading error (order 0.1-0.2 G-value units) on top
  of whatever the paper's own figures already carry.

This directory supersedes the earlier session-local indicative check for Farokhi2023
(`.work/sessions/2026-10-09__farokhi2023-flash-oxygen-depletion-chemistry-layout-90-mev/analysis/`),
which used smaller smoke-test runs (20/50 events, 3-4 points/decade) without SEM or a
statistics check.
