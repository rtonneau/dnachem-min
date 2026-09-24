# dnachem-min

Minimal Geant4-DNA example for studying water radiolysis from electron beams.
It provides the common physics, chemistry, and scoring basis for
conventional and ultra-high-dose-rate (UHDR) irradiation studies. UHDR is a
use case, not a separate implementation — there is no dedicated UHDR
pulse-structure, dose-rate-effect, or delivery-model code, and none is
planned unless the project scope changes.

The geometry is a single homogeneous water box (no voxelization).

## Requirements

- Geant4 11.0+ with DNA models
- CMake 3.16+
- A C++20 compiler
- HDF5 with C++ support

## Build

See `.claude/geant4-instructions.md` for the full MSVC build procedure
(the `build/` vs `build-ninja/` split, background runs, verification
checklist). In short: `build/` is a RelWithDebInfo tree used to run `sim`;
`build-ninja/` is a Debug tree used to run the unit tests via CTest.

## Run

From the run build directory (`build/`), pass the macro **filename only** —
`sim.cc` prepends `macro/` itself, and the build copies `macro/` alongside
the executable:

```bash
./sim beam.in             # pure-water radiolysis
./sim beam_o2.in          # with dissolved-O2 scavenger config (see caveat below)
./sim reaction_counter.in # reaction-counting example, see below
```

Other flags:

```bash
./sim beam.in --threads 4       # multithreaded run (default: Serial)
./sim beam.in --dir runs/001    # redirect all output files into runs/001
```

`--dir <path>` can appear anywhere on the command line, but the macro
filename must still come first. `<path>` is created if missing; its parent
must already exist.

The macro command `/run/outputDir <path>` (`PreInit` only, before
`/run/initialize`) sets the same output directory from inside a macro
instead of the command line:

```text
/run/outputDir runs/001

/run/initialize
```

`--dir` is always applied before the macro runs, so if a macro sets a
*different* path than `--dir` did, the run aborts with a fatal
configuration error instead of silently picking one. Setting the same path
from both is a harmless no-op.

## Output files

Each run writes:

- `Species.Txt` — human-readable species yields vs. time
- `Species_nt_species.csv` — aggregate `sumG`/`sumG2` per species/time (nt = NTuple)
- `Species_nt_species_all.csv` — same, per event
- `Reactions.Txt`, `Reactions_nt_reactions.csv`, `ReactionsMetadata.csv` —
  reaction-firing counts (see [Reaction counter](#reaction-counter) below)
- `output_event_<n>.txt` (Serial) or `output_event_t<thread>_e<event>.txt`
  (MT) — per-event pre-chemical dumps

Multiple `/run/beamOn` calls in one macro overwrite `Reactions.Txt` and
produce `*_bis.csv` names for the later ntuples, same as the species files.

## Logging

The project uses a custom `DnaLogger` instead of ad hoc console output:

```text
/dnaLogger/verbose Quiet|Error|Warning|Info|Debug|Trace
```

Default is `Error` (quiet). Use `Info` or `Debug` to see progress and
diagnostic messages, e.g. the reaction-counter write confirmation below.

## Chemistry

Species and reactions are defined in code
(`src/DnaChemistryList.cc`/`src/PureWaterReactions.cc`), not via macro
commands — `/chem/species` and `/chem/reaction/add` are intentionally not
used (`/chem/reaction/UI` would reset the shared reaction table and wipe the
class-built one).

The pH-driven acid-base buffer network and the full O2⁻/HO2/HO2⁻/O⁻/O3⁻
network are **baseline and always on** — pure water radiolysis alone can
produce O2 through this network. `/chem/env/O2 <percent>` is currently a
no-op reserved for a future *exogenous* dissolved-O2 supply mechanism;
`/chem/env/pH <double>` is active and drives the bulk `H3Op(B)`/`OHm(B)`
buffer concentration. See `CONTEXT.md` and
`docs/adr/0001-baseline-acid-base-buffer.md` for the full vocabulary and
rationale.

Only SBS is supported as the chemistry time-step model
(`/process/chem/TimeStepModel SBS`); IRT is rejected with a fatal exception.

The chemistry time limit defaults to 1 µs
(`G4Scheduler::Instance()->SetEndTime()` in `ActionInitialization::Build()`,
applied on `/run/initialize`). To override it, issue
`/scheduler/endTime <value> <unit>` *after* `/run/initialize` — anything
issued before that point is overwritten.

## Reaction counter

`ReactionCounter` (`src/ReactionCounter.cc`) counts, per time bin, how many
times each bimolecular reaction fires — live, during the run, via
`TimeStepAction::UserReactionAction`. Counts are merged across worker
threads and aggregated over all events in the run. Acid-base/scavenger
reactions never reach that hook, so they are not counted.

### Configuring the time bins

Both commands are `PreInit` (issue them before `/run/initialize`) and are
mutually exclusive — issuing both in the same macro is a fatal
configuration error:

- `/chem/reaction/timeBinsFixed <width> <unit>` — a fixed step, expanded up
  to the chemistry scheduler's end time.
- `/chem/reaction/timeBinsList <e1> <e2> ... <eN> <unit>` — explicit bin
  edges.

If neither is issued, a built-in 7-edge default table is used: 1, 10, 100,
1000, 10000, 100000, 999999 ps.

`/chem/reaction/dump <filename>` writes the full reaction table (bimolecular
+ acid-base networks) to `<filename>` for inspection — also `PreInit`.

### Output format

- `Reactions.Txt` — human-readable, one block per time bin, each reaction
  printed with its full `"A + B -> C + D"` label and firing count.
- `Reactions_nt_reactions.csv` — rows of `(reactionId, time, count)`.
- `ReactionsMetadata.csv` — maps each `reactionId` to its full
  `"A + B -> C + D"` label.

The `[RunAction] reaction counts written...` confirmation line is logged at
`Info` level, so it's silent unless `/dnaLogger/verbose Info` (or more
verbose) is set.

### Example: `macro/reaction_counter.in`

```text
/run/verbose 0
/tracking/verbose 0
/dnaLogger/verbose Info

/process/dna/e-SolvationSubType Ritchie1994
/process/chem/TimeStepModel SBS

# Bin reaction counts at a fixed 100 ps step (PreInit, before /run/initialize).
/chem/reaction/timeBinsFixed 100 picosecond

/run/initialize

/gun/particle e-
/gun/energy 100 keV

/run/beamOn 4
```

Run it with:

```bash
./sim reaction_counter.in
```

This produces `Reactions.Txt`, `Reactions_nt_reactions.csv`, and
`ReactionsMetadata.csv` with counts binned every 100 ps out to the 1 µs
scheduler end time (~10 bins), plus the usual `Species.*` output.

To use explicit bin edges instead, comment out `timeBinsFixed` and use, e.g.:

```text
/chem/reaction/timeBinsList 1 10 100 1000 picosecond
```

## Testing

Unit tests (`test/*Test.cc`, plain `assert` + CTest) are built and run from
`build-ninja/` (Debug). See `.claude/geant4-instructions.md` section 5 for
the full procedure, including `NDEBUG` and Debug-CRT-dialog pitfalls.

## Repo layout

- `sim.cc` — application entry point, CLI flags (`--threads`, `--dir`) and
  macro-command messengers (`--dir`'s counterpart, `/run/outputDir`, lives
  in `src/OutputDirMessenger.cc`)
- `src/`, `header/` — implementation and headers (`DetectorConstruction`,
  `PhysicsList`, `DnaChemistryList`, `PureWaterReactions`,
  `DnaChemistryWorld`, `ReactionCounter`, `ScoreSpecies`, `TimeStepAction`,
  `DnaLogger`, …)
- `macro/` — runtime beam and chemistry configuration macros
- `test/` — unit tests (CTest, no Geant4 kernel dependency)
- `docs/adr/` — architecture decision records
- `CONTEXT.md` — chemistry terminology glossary
- `CLAUDE.md` — full project conventions and agent instructions

## Further reading

- `CONTEXT.md` — chemistry vocabulary (pure-water chemistry, scavenger, bulk
  species)
- `docs/adr/0001-baseline-acid-base-buffer.md` — why the acid-base/O2
  network is baseline rather than opt-in
- `CLAUDE.md` — full build/run/test procedure references, coding
  conventions, and agent workflow
