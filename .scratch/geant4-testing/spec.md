Status: ready-for-agent

# Test infrastructure for Geant4-DNA chemistry reproducibility

## Problem Statement

This project has no automated way to verify that a code change doesn't break
physics correctness or crash the application. Results also aren't reproducible
today: nothing anywhere sets an RNG seed, so two runs of the same macro never
produce comparable output. This matters especially because AI agents actively
modify core chemistry code (species sets, reaction rates, process wiring) and
can introduce subtle correctness bugs that only manual inspection currently
catches — the previous session's chemistry refactor found and fixed a
scavenger-process registered against the wrong `G4MoleculeDefinition`, a bug
that had been silently unreachable and would not have been caught without a
human manually reading `Species.Txt` after a run.

## Solution

Add a lightweight, locally-invocable (`ctest`) test harness with three tiers —
compile check, smoke test, exact-match regression test — driven by a fixed RNG
seed and a checked-in golden CSV file, so both AI agents and the maintainer can
run `ctest` after any change and get a pass/fail signal without manually
running the executable and eyeballing output. Alongside this: add explicit RNG
seed control (currently absent anywhere in the codebase), switch the
analysis-manager output format from ROOT to CSV project-wide, remove the
unused G4Vox dependency entirely, and guard Geant4's documented MT-event
reproducibility property with a second regression case run at a higher thread
count against the same golden file.

## User Stories

1. As an AI agent making a code change, I want a single command (`ctest`) that verifies the project still compiles, so that I catch build-breaking mistakes before reporting work complete.
2. As an AI agent making a code change, I want a smoke test that runs a short macro and asserts no fatal `G4Exception`/crash, so that I catch wiring bugs without a human needing to manually run and inspect output.
3. As an AI agent making a chemistry change, I want an exact-match regression test against a checked-in golden CSV, so that any change to reaction rates, species sets, or process wiring is caught automatically, whether intended or not.
4. As the maintainer, I want RNG seeding to be explicit and deterministic by default, so that "reproducibility" is achievable rather than aspirational.
5. As the maintainer, I want the regression test to run single-threaded by default, so floating-point summation order across events (which is not guaranteed identical run-to-run under MT, since worker completion order depends on OS scheduling) doesn't cause spurious exact-match failures unrelated to real physics changes.
6. As the maintainer, I want a second regression case that runs the same macro at a higher thread count against the same golden file, so that a future change breaking Geant4's documented per-event MT-reproducibility guarantee is caught automatically instead of silently assumed.
7. As the maintainer, I want all test infrastructure to run locally without GitHub Actions or any hosted CI, so heavy Geant4/Qt/HDF5/G4Vox dependency provisioning isn't a blocker to getting a basic safety net in place.
8. As the maintainer, I want the G4Vox dependency removed entirely, not just avoided, so the build has one fewer unused heavy dependency, since project convention already forbids using it.
9. As the maintainer, I want species-yield output switched from ROOT to CSV project-wide, so both test scripts and any future ad hoc analysis can read results without ROOT-specific tooling.
10. As an AI agent, I want the regression test's comparison target to be the smallest complete artifact (the aggregate `species` ntuple, not the larger per-event `species_all` ntuple), so golden-file diffs stay easy to read when a test fails.
11. As the maintainer, I want a documented, deliberate process for regenerating the golden file when a chemistry change is intentional, so the regression test doesn't become a maintenance burden that gets bypassed or ignored.
12. As an AI agent, I want the harness discoverable via one standard command (`ctest --test-dir build`), so I don't need project-specific tribal knowledge to run tests.
13. As the maintainer, I want the smoke and regression tests to use a dedicated test macro, not the existing example macros (`beam.in`/`beam_02.in`/`beam_o2.in`), so adding a fixed seed and a reduced event count for test speed doesn't compromise those macros' role as usage documentation.
14. As an AI agent debugging a failing regression test, I want the comparison script's failure output to name the specific species/time/value that diverged, so I can tell whether the failure is a real bug or an intentional physics change needing golden-file regeneration.
15. As the maintainer, I want the regression golden file's O2-derived species (Om, HO2, etc.) appearing even in the pure-water-only test macro to be self-explanatory from the golden-file documentation, so a future reviewer doesn't mistake this for a bug — it follows directly from the baseline acid-base network established in `docs/adr/0001-baseline-acid-base-buffer.md`.

## Implementation Decisions

- **RNG seeding**: add an explicit, macro-settable RNG seed, with a fixed (not
  time-based) default so an unseeded run is still reproducible. Nothing in the
  codebase sets a seed today.
- **Dedicated test macro**: a new macro distinct from the example macros,
  fixed seed, reduced event count for speed. Both the smoke test and the
  regression test reuse it.
- **Output format**: change the analysis-manager output format from `"root"`
  to `"csv"` as the project-wide default, not test-scoped. This produces two
  structured ntuples (`species`: aggregate sumG/sumG2 per species/time;
  `species_all`: same, per event) in place of the ROOT file. The existing
  plain-text species dump is a fully independent code path and is unaffected
  either way.
- **Comparison target**: the regression test compares the aggregate `species`
  ntuple CSV against a checked-in golden file — the smallest complete
  artifact, not the larger per-event ntuple.
- **Threading**: the primary regression case runs the test macro with a
  single-thread override. A second regression case runs the identical macro
  and golden-file comparison at a higher thread count, standing guard on
  Geant4's documented per-event MT-reproducibility property as an ongoing
  regression target in its own right, rather than a one-off manual check.
- **Harness mechanics**: CTest test cases registered in the CMake build,
  each invoking a small Python comparison script that runs the executable
  against the test macro and diffs the resulting CSV exactly against the
  golden file. The smoke test only checks process exit status / absence of a
  fatal exception, not output content.
- **Compile check**: implicit — `ctest` requires a successful prior build; no
  separate CTest case is needed beyond documenting that a build must precede
  `ctest`.
- **G4Vox removal**: remove the dependency entirely from the build
  configuration (not just avoid using it). Verified unreferenced anywhere in
  source except a stale doc-comment in the detector-construction module,
  which should be corrected to describe its actual purpose (homogeneous
  water-box geometry) rather than left referencing voxel use.
- **Golden-file regeneration**: a documented, explicit, manual procedure for
  regenerating the golden file when a chemistry change is intentional (rerun
  the test macro, review the new CSV output, replace the checked-in file).
  Deliberately not automated — an unreviewed auto-regeneration would defeat
  the point of an exact-match regression test.
- **Vocabulary/ADR grounding**: golden-file values will reflect the baseline
  pure-water + O2-derived + acid-base-buffer chemistry established by
  `docs/adr/0001-baseline-acid-base-buffer.md` and `CONTEXT.md`'s "pure-water
  chemistry" vs "scavenger" distinction — species like Om/HO2 appearing in the
  pure-water-only test macro's golden output is expected, not a bug, and the
  golden-file documentation should say so explicitly.

## Testing Decisions

- **What makes a good test here**: only external behavior is asserted —
  process exit status and macro-produced output files (CSV, `Species.Txt`) —
  never internal C++ function calls or intermediate state. This follows
  directly from the seam decision below.
- **The seam**: the compiled executable invoked with a macro file is the only
  practical seam. Geant4-DNA's architecture is singleton-heavy
  (`G4MoleculeTable::Instance()`, `G4RunManager`, etc.), populated only
  through full physics-list construction — even the deliberately
  self-contained `PureWaterReactions` builder still requires
  `G4ChemDissociationChannels_option1::ConstructMolecule()` to have already
  run. No narrower C++-level unit seam is practical; this was confirmed with
  the maintainer rather than assumed.
- **Modules exercised**: effectively the full compiled pipeline end-to-end
  (physics list, chemistry list, reaction table, scoring) through one seam,
  for all three test tiers.
- **Prior art**: none — this is the first test infrastructure introduced in
  this repo.

## Out of Scope

- GitHub Actions or any hosted CI — local/agent-invoked `ctest` only.
- A literature/validation tolerance-band test tier (comparing G-values against
  published benchmarks) — deferred; requires domain curation of tolerance
  bands that only the maintainer can do correctly.
- Testing the deferred future O2-scavenger file — it doesn't exist yet.
- Comparing the per-event `species_all` ntuple — only the aggregate `species`
  ntuple is part of the regression check.
- Any change to the existing example macros (`beam.in`, `beam_02.in`,
  `beam_o2.in`) — a new, separate test macro is used instead.
- Unit-testing individual C++ classes/functions in isolation — impractical
  given the singleton-heavy Geant4-DNA initialization model (see Testing
  Decisions).

## Further Notes

- This spec follows a grilling session that settled all 7 decisions listed
  above before this spec was written; there should be no open physics or
  architecture judgment calls left for the implementing agent. Minor
  mechanical choices not meaningful enough to be user decisions (the exact
  seed value, exact event count, exact CTest case names) are left to the
  implementing agent's discretion.
- The single-thread-default-with-MT-guard threading design (decisions 5-6) is
  arguably itself ADR-worthy once implemented (hard to reverse once a golden
  file exists, non-obvious without explanation, a genuine three-way
  trade-off) — flagging this as a candidate for a follow-up ADR rather than
  creating one now, since this spec was scoped to referencing prior-session
  ADRs, not authoring new ones.
- Status is `ready-for-agent` per `docs/agents/triage-labels.md`: fully
  specified, no outstanding physics or judgment call blocking implementation.
- **Addendum, found during ticketing**: two premises above were verified and
  found inaccurate. (1) The claim that results are non-reproducible today was
  only half right — `sim.cc`/CLHEP's default RNG engine is already
  deterministic (verified: two unseeded runs produced byte-identical
  `Species.Txt`); the real gap was that this determinism was implicit and
  undocumented, not that it didn't exist (see ticket 02). (2) `sim.cc`
  currently hardcodes `G4RunManagerType::Serial` — it does **not** run
  multithreaded by default, contradicting `CLAUDE.md`'s existing "MT by
  default" claim. An extra ticket (06, between the single- and multi-thread
  regression tests) adds a CLI-driven opt-in to MT mode, since nothing could
  otherwise switch the run manager to MT at runtime, and fixes the stale
  `CLAUDE.md` claim. See `.scratch/geant4-testing/issues/` for the full,
  corrected 7-ticket breakdown and their exact acceptance checks.

- **Addendum, found while implementing ticket 02**: the (1) premise above was
  itself wrong, not just under-verified. Re-checked directly (not assumed):
  two separate `beam.in` process launches produce **different** `Species.Txt`
  (real species-count divergence, not reordering), with or without an
  explicit fixed `G4Random::setTheSeed(...)` set as the first statement in
  `main()`. Isolated with `superpowers:systematic-debugging` (see ticket 02's
  own addendum for the full trace) to: the pre-chemistry physics/tracking
  stage *is* reproducible under a fixed seed (identical track counts/content
  across runs); the chemistry (IT) stepping stage downstream of that
  identical input still diverges. Traced partway into Geant4's own installed
  DNA-chemistry kernel source (not `dnachem-min` code) — ruled out one
  specific container (`G4ITReactionPerTrackMap`, actually TrackID-ordered,
  not pointer-ordered) but did not reach the exact divergence point before
  stopping, per the maintainer's call to treat this as a known Geant4-DNA
  kernel limitation rather than continue root-causing.
  **This invalidates the exact-match regression-test premise behind tickets
  05 and 07** (both `Implementation Decisions` above assume process-to-process
  bit-exact reproducibility is achievable — it is not, at least not without
  further kernel-level work or a different test design). Tickets 02, 05, and
  07 are marked `ready-for-human` pending a maintainer decision on how to
  redefine "reproducible" for this project's test suite (tolerance-based
  comparison, restrict to a configuration where the divergence doesn't
  manifest, resume kernel root-causing, or something else) — this is
  explicitly the kind of judgment call the spec's own "Out of Scope" section
  reserves for the maintainer, not an implementing agent.
