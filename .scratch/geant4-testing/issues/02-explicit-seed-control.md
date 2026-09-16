Status: ready-for-human

# Make RNG determinism explicit and documented in sim.cc

Spec: `.scratch/geant4-testing/spec.md` (Implementation Decisions, "RNG
seeding"). Depends on: 01 (build should be G4Vox-free before further changes,
though this ticket doesn't touch `CMakeLists.txt`).

## Context — important correction to the spec's framing

The spec's problem statement says results aren't reproducible because nothing
sets an RNG seed. **This was verified empirically this session and found to
be only half right**: running `beam.in` twice unseeded, back to back, and
diffing `Species.Txt` produced **zero differences** — Geant4/CLHEP's default
RNG engine already starts from a fixed, non-time-based seed in this build, so
the app is *already* deterministic by accident. Nothing in `sim.cc` or
`RunAction.cc` sets a seed explicitly, so this determinism is an undocumented
property of the CLHEP default engine, not a deliberate design choice.

Also verified: `/random/setSeeds <a> <b>` is a **standard Geant4 UI command**,
registered automatically by `G4RunMessenger` (part of the kernel every
`G4RunManager`/`G4MTRunManager` instantiates) — it already works from any
macro today with zero code changes. No new messenger needs to be written for
macro-level override.

Given this, the actual work here is narrower than "add seed control": make
the existing implicit determinism an explicit, documented, intentional
property of the application, so a future reader isn't relying on an
undocumented CLHEP default that could silently change with a CLHEP/Geant4
version upgrade.

## Task

In `sim.cc`, before any macro is executed (before `UImanager->ExecuteMacroFile(...)`
or equivalent), call the engine seed-setting function (e.g.
`CLHEP::HepRandom::setTheSeed(...)` or `G4Random::setTheSeed(...)` — confirm
which one this Geant4 version's `G4Random.hh` exposes) with a named constant
(e.g. `constexpr long kDefaultSeed = 12345;`), documented with a one-line
comment explaining this makes existing implicit determinism explicit rather
than introducing new behavior. Do not add a new messenger — `/random/setSeeds`
already exists and needs no wiring.

## Acceptance check

Default (unseeded-by-macro) determinism still holds after this change:
```
cd build
./sim.exe beam.in
copy Species.Txt Species_run1.Txt
./sim.exe beam.in
copy Species.Txt Species_run2.Txt
fc Species_run1.Txt Species_run2.Txt
```
Expected: `fc` reports no differences (files are identical) — same result as
verified pre-change, confirming the explicit seed didn't change behavior.

Macro-level override actually takes effect:
```
# create a temp macro identical to beam.in but with this line inserted
# before /run/initialize:
#   /random/setSeeds 111 222
./sim.exe temp_seeded.in
copy Species.Txt Species_seeded1.Txt
./sim.exe temp_seeded.in
copy Species.Txt Species_seeded2.Txt
fc Species_seeded1.Txt Species_seeded2.Txt
fc Species_run1.Txt Species_seeded1.Txt
```
Expected: `Species_seeded1.Txt` and `Species_seeded2.Txt` are identical to
each other (override is itself deterministic), but **different** from
`Species_run1.Txt` (proving the override actually changes the RNG stream, not
a no-op).

## Addendum — acceptance check does NOT pass as originally written; needs a human decision

Implemented (kept, uncommitted-pending-this-decision): `G4Random::setTheSeed(12345)`
added in `sim.cc`, first statement in `main()`, before any Geant4 kernel object
exists. Verified against the actual acceptance check above and found the
ticket's premise itself was wrong, not just under-verified:

- The **first** acceptance check ("two `beam.in` runs produce identical
  `Species.Txt`") **fails**, both with and without this seed change. Isolated
  with a reduced 1-event repro (`macro/test_seed_debug.in`, ~2s/run): the
  pre-chemistry physics/tracking stage *is* fully reproducible under the
  fixed seed (identical "Size of pre chemical main list" track counts across
  separate process launches, all 6 events, byte-for-byte) — but the
  chemistry (IT) stepping stage that consumes that identical input still
  diverges run-to-run (confirmed even for a single event: final chemistry
  time 7812.1 ps vs 4411.1 ps across two runs of the same seeded macro).
- Ran via `superpowers:systematic-debugging`. Traced into Geant4's own
  installed DNA-chemistry kernel source
  (`processes/electromagnetic/dna/{management,models}`), not project code:
  ruled out `G4ITReactionPerTrackMap` (it's `std::map<G4Track*, ...,
  compTrackPerID>` — ordered by `GetTrackID()`, not raw pointer, so
  deterministic) as the cause; found no separate/unseeded RNG engine in the
  DNA module (chemistry uses the same global `G4Random` engine as physics);
  confirmed `dnachem-min`'s own `src/` has no pointer-keyed containers that
  could explain it. Did not reach the exact line — likely somewhere in
  `G4DNASmoluchowskiReactionModel`/`G4DNABrownianTransportation`/spatial
  reactant search, inside Geant4's own kernel, not `dnachem-min` code.
- **This is a genuine property of the installed Geant4-DNA IT chemistry
  kernel in this build**, not a bug introduced by this ticket or by ticket
  01. It undermines the exact-match-regression premise behind tickets 05 and
  07 too (both now marked `ready-for-human`, not just this one) — see
  `spec.md`'s addendum.
- Decision needed from the maintainer before this ticket (or 05/07) can be
  re-scoped: continue kernel-level root-causing, accept the physics-stage-only
  determinism this ticket already achieves and redefine what "reproducible"
  means for this project, or something else. Not an agent-level call — spec
  explicitly reserves tolerance/validation-design judgment calls for the
  maintainer (see spec.md "Out of Scope").
