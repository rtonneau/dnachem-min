Status: ready-for-human

## Blocked — see ticket 02's addendum and spec.md's addendum

This ticket's exact-match premise is invalidated: `beam.in`-style runs are
not process-to-process reproducible even with an explicit fixed RNG seed —
the chemistry (IT) stepping stage diverges run-to-run for reasons traced
partway into Geant4's own kernel (not `dnachem-min` code) but not fully
root-caused. Needs a maintainer decision on test design (tolerance-based
comparison, a different seam, resuming kernel root-causing, etc.) before an
agent can implement this ticket as originally scoped.

# Python comparison script + golden file + single-thread regression test

Spec: `.scratch/geant4-testing/spec.md` (Implementation Decisions,
"Comparison target"; "Harness mechanics"; "Golden-file regeneration"; User
Stories 3, 11, 14). Depends on: 04.

## Task

1. Create a dedicated test macro `macro/test_pure_water.in` (lives alongside
   the existing example macros so it's picked up by the project's existing
   `copy_directory macro/` build step — no new CMake copy rule needed). Base
   it on `macro/beam.in`'s structure but: an explicit `/random/setSeeds <a> <b>`
   call before `/run/initialize` (don't rely on `sim.cc`'s default seed from
   ticket 02 staying unchanged — make the regression test self-contained),
   `/run/numberOfThreads 1` explicitly, and **exactly one** `/run/beamOn N`
   call with a small N (e.g. 2) — avoids the `_bis`-filename quirk found in
   ticket 03 and keeps the test fast.
2. Create `tests/compare_species.py`: runs `sim.exe macro/test_pure_water.in`
   from the build directory, parses `Species_nt_species.csv` (skip lines
   starting with `#`, comma-separated data rows — exact format verified in
   ticket 03), and compares every row exactly against a checked-in golden
   file. On mismatch, the script's output must name the specific row (species
   name + time) and column that diverged, with both the golden and actual
   values — a plain "files differ" message is not acceptable (spec User
   Story 14).
3. Create `tests/golden/pure_water_species.csv`: the golden file, generated
   by running `macro/test_pure_water.in` once and reviewing the output before
   checking it in. Expect to see O2-derived species (Om, HO2, etc.) in this
   pure-water-only macro's output — that's expected per
   `docs/adr/0001-baseline-acid-base-buffer.md`, not a bug; say so in a
   comment at the top of the golden file or in `tests/README.md`.
4. Create `tests/README.md`: short, explicit golden-file regeneration
   procedure (rerun the script with a `--update-golden` flag, or documented
   manual steps — implementer's choice, but it must be a reviewed, deliberate
   action, never automatic).
5. Register a `regression_st` CTest case in `CMakeLists.txt` that invokes
   `tests/compare_species.py`.

## Acceptance check

```
cd build
cmake --build . --config RelwithDebInfo
ctest --test-dir . -R regression_st -V
```
Expected: `Passed`, verbose output shows the comparison script ran and
reports something like "0 rows differed" (exact wording is the implementer's
choice, but it must be unambiguous about pass/fail).

Negative check — confirm the script actually detects divergence and names it:
```
# manually edit tests/golden/pure_water_species.csv: change one sumG value
# for one species/time row to an obviously wrong number
ctest --test-dir . -R regression_st -V
```
Expected: `Failed`, and the verbose output names the specific species and
time whose value diverged, showing both the golden and actual numbers — not
just "mismatch" or a raw diff. Then revert the golden-file edit and confirm
`ctest --test-dir . -R regression_st` passes again.
