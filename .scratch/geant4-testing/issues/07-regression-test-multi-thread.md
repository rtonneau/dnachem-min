Status: ready-for-agent

# Multi-thread regression case, guarding MT-reproducibility itself

Spec: `.scratch/geant4-testing/spec.md` (Implementation Decisions,
"Threading"; User Stories 6, 15). Depends on: 06.

## Context

Geant4's `G4MTRunManager` is documented to pre-assign each event's RNG seed
on the master before dispatching to workers, specifically so per-event
physics results are reproducible independent of thread count and OS
scheduling. This hasn't been verified for this application. This ticket both
verifies it once and turns that verification into a standing regression
guard: if a future change (in this codebase or a Geant4 upgrade) breaks that
guarantee, this test starts failing instead of the assumption silently
rotting.

Note: aggregate sums (`sumG`/`sumG2` in the `species` ntuple) are computed by
summing across events at run-merge time. Under MT, the order in which worker
threads complete (and therefore the order values are summed in) is not
guaranteed identical run-to-run, and IEEE 754 floating-point addition is not
associative — so even if every individual event's physics is bit-identical,
the aggregate sums could theoretically differ in trailing digits between two
MT runs. If this test turns out to be flaky for that reason (not a real
physics bug), that's a real finding — report it rather than silently loosening
the comparison to a tolerance band, since that changes the design this spec
settled on.

## Task

1. Reuse `macro/test_pure_water.in`, `tests/compare_species.py`, and
   `tests/golden/pure_water_species.csv` from ticket 05 unchanged — do not
   duplicate them.
2. Register a second CTest case `regression_mt` that invokes the same
   comparison script but runs `sim.exe` with a thread-count argument (e.g.
   `sim.exe macro/test_pure_water.in 4`, using ticket 06's new CLI argument)
   instead of the default single-threaded invocation, comparing against the
   **same** golden file as `regression_st`.
3. If step 2 turns out to be flaky (fails intermittently across repeated
   runs) due to the floating-point summation-order issue described above,
   stop and report this rather than silently working around it — it's a
   genuine finding that changes the design (the maintainer decided single-
   thread-by-default specifically to avoid this risk; if it manifests even in
   the guard test, that's worth knowing, not hiding).

## Acceptance check

```
cd build
cmake --build . --config RelwithDebInfo
ctest --test-dir . -R regression_mt -V
```
Expected: `Passed`, comparing MT output against the same golden file
`regression_st` uses.

Flakiness check — run it several times back to back, since a single pass
doesn't rule out the summation-order risk:
```
for /L %i in (1,1,5) do ctest --test-dir . -R regression_mt
```
Expected: passes all 5 times. If any run fails, do not adjust the comparison
tolerance to paper over it — report the failure pattern (which
species/time/column diverged, and whether it recurs) instead.

Full suite sanity check:
```
ctest --test-dir .
```
Expected: `100% tests passed, 0 tests failed out of 4` (`compile`, `smoke`,
`regression_st`, `regression_mt` — using whatever exact names tickets 04/05
gave their cases).
