# dnachem-min

## Purpose and Scope

Minimal Geant4-DNA example for studying water radiolysis from electron beams. It provides the common physics, chemistry, and scoring basis for conventional and ultra-high-dose-rate (UHDR) irradiation studies.

UHDR is a use case, not a separate implementation: do not add UHDR-specific pulse structure, dose-rate effects, delivery models, or dedicated physics/chemistry models unless the project scope changes.

The geometry is one homogeneous water box. Do not introduce voxelization or G4Vox-based geometry.

## Build and Run

Requirements: Geant4 11.0+ with DNA models, CMake 3.16+, a C++20 compiler, and HDF5 with C++ support.

How to build, run and test (MSVC environment, the `build/` vs `build-ninja/` split, background runs, verification checklist) is in `.claude/geant4-instructions.md`; the values it uses are the `build`, `run` and `test` sections of `.claude/.claude-project.json`. Follow it instead of re-deriving the procedure.

Project-specific: `build/` is RelWithDebInfo (run `sim`), `build-ninja/` is Debug (ctest). `sim.cc` prepends `macro/` to the macro argument itself, so pass the filename only, from the run build dir (the build copies `macro/` there):

```bash
./sim beam.in      # pure-water radiolysis
./sim beam_o2.in   # with dissolved-O2 scavenger
```

Species/reaction/energy/physical-interaction data accumulates across `/run/beamOn`
calls rather than being written and reset after every run — species yields
accumulate automatically (the `ScoreSpecies` scorer is itself a persistent,
SD-registered object), while energy deposit and the two counters accumulate via
`RunAccumulator` (`src/RunAccumulator.cc`), fed once per run from
`RunAction::EndOfRunAction`. Nothing is written to disk until a macro issues
`/run/dumpDataAndReset [prefix]` (`Idle` state, i.e. between `beamOn` calls),
which writes everything accumulated since the last dump (or program start) and
resets all counters to empty/zero:

- `Species.Txt` (human-readable species yields vs. time) and two CSV ntuples,
  `Species_nt_species.csv` (aggregate sumG/sumG2 per species/time) and
  `Species_nt_species_all.csv` (same, per event); in MT mode the per-event
  pre-chemical dumps are still written continuously as
  `output_event_t<thread>_e<event>.txt` (unaffected by dump/reset).
- `Reactions.Txt`, `Reactions_nt_reactions.csv`, and `ReactionsMetadata.csv`:
  per-time-bin firing counts of each bimolecular reaction (counted live in
  `TimeStepAction::UserReactionAction` via `ReactionCounter`, merged across
  threads in `Run::Merge`, accumulated across runs by `RunAccumulator`).
  `Reactions_nt_reactions.csv` rows are `(reactionId, time, count)`;
  `ReactionsMetadata.csv` maps each `reactionId` to its full `"A + B -> C + D"`
  label (`Reactions.Txt` still prints the full label directly). Acid-base/
  scavenger reactions are not counted (they never reach that hook). The
  default time-bin edges are a built-in 7-entry table; override them with
  `/chem/reaction/timeBinsFixed <width> <unit>` (a fixed step, expanded up to
  the chemistry scheduler's end time) or `/chem/reaction/timeBinsList <e1>
  <e2> ... <eN> <unit>` (explicit edges) — both `PreInit`, mutually exclusive
  (last one issued wins).
- `EnergyDeposit.Txt` (total energy deposited in the simulation volume,
  human-readable).
- `PhysicsInteractions.Txt`/`PhysicsInteractions.csv` (per-process physical-
  interaction firing counts — totals only, no time binning; only discrete
  G4DNA physics processes are counted, e.g. `e-_G4DNAIonisation`,
  `e-_G4DNAExcitation`, `e-_G4DNAElastic`, `e-_G4DNAVibExcitation`,
  `e-_G4DNAAttachment` — `Transportation` and other bookkeeping steps are
  excluded). `PhysicsInteractionCounter` is recorded live per step by
  `SteppingAction`, merged across worker threads in `Run::Merge`, and
  accumulated across runs by `RunAccumulator`, same as the reaction counts.

`prefix` (optional, default none) is prepended literally to every filename
above — no separator is inserted, so pass e.g. `run1_` if you want one.
Reusing a prefix already used earlier in the same `sim.exe` process is a
fatal error (`RunAccumulator::TryReservePrefix`), to catch accidental
overwrites. Each dump also calls `G4AnalysisManager::Clear()` after writing
its CSV ntuples — without it, a later dump cycle reusing the same ntuple
names ("species"/"reactions") under a new prefix hits a Geant4 analysis-
manager limitation (its per-ntuple file registry isn't cleared by
`CloseFile()` alone) and silently renames or drops that dump's CSV output;
`Clear()` avoids that entirely, so distinct prefixes never collide.

`/run/dumpDataAndResetToDir <subdir>` (`Idle` state, parameter required) is the
same dump, but writes the unprefixed files into `<subdir>` under the output
directory (`--dir` / `/run/outputDir`, or cwd if none) instead of using a
filename prefix — e.g. `/run/beamOn 4`, `/run/dumpDataAndResetToDir run01`,
`/run/beamOn 4`, `/run/dumpDataAndResetToDir run02` yields `run01/` and
`run02/`, each with the full file set. The folder is created if missing
(`OutputDir::ConfigureSubdir`); nested names such as `scan1/run01` are allowed,
absolute paths and `..` components are a fatal `G4Exception`
(`InvalidDumpSubdir`). Reusing a name within the same process is a fatal
`G4Exception` (`DuplicateDumpSubdir`, `RunAccumulator::TryReserveSubdir` — a
set separate from the prefix one); a folder already on disk from an earlier
process is reused and its files overwritten. It does not combine with a
prefix. The MT per-event `output_event_t*_e*.txt` files stay in the top output
directory (they are written continuously, outside any dump).

If a macro never issues `/run/dumpDataAndReset` and there is still accumulated
data pending when the program is about to exit, a safety-net flush fires
automatically with the fixed prefix `EndOfRun_` (see
`RunAccumulatorMessenger::FlushIfPending`, called from `sim.cc` just before
the run manager is destroyed) — so data is never silently lost even if the
operator forgets the manual call. The `[RunAccumulatorMessenger] dumped and
reset...` line is a plain, always-visible `G4cout` line (unlike most of this
project's diagnostics, which go through `DnaLogger` and are silent by
default) — see `RunAccumulatorMessenger.cc`.

Pass `--dir <path>` to redirect every output file above (plus the
`/chem/reaction/dump` target, if the macro sets one) into `<path>` instead of
cwd — useful for isolating each run's output when scripting many `sim.exe`
invocations. `<path>` itself is created if missing; its parent must already
exist. `--dir` can appear anywhere on the command line, but the macro
filename must still come first (`argv[1]`):

```bash
./sim beam.in --dir runs/001
```

The macro command `/run/outputDir <path>` (`PreInit` only) is a second way
to set the same output directory, for when `--dir` isn't convenient (e.g. a
self-contained macro). `--dir` is always applied before the macro executes,
so if both are used with *different* paths, `/run/outputDir` raises a fatal
`G4Exception` (`ConflictingOutputDir`) and aborts the run rather than
silently picking one; the same path from both is a harmless no-op.

## Key Files

- `sim.cc`: application setup. Runs Serial by default; `--threads N` (N > 0) selects the MT run manager (`G4RunManagerType::MT`), and `/run/numberOfThreads N` in a macro can still override before `/run/initialize`. Set `useGUI` to `true` only for interactive GUI runs. CLI flags (`--threads`, `--dir`) are registered here via `src/ArgParser.cc`.
- `src/OutputDir.cc`: process-wide output directory (`--dir`), configured once in `main()` before any worker thread starts. `Resolve(filename)` is called at every output-file site (`ScoreSpecies.cc`, `TimeStepAction.cc`, `DnaChemistryList.cc`'s reaction-table dump) to prefix it onto the configured directory, or leaves it unchanged when `--dir` wasn't passed. `ConfigureFromMacro()` backs the `/run/outputDir` macro command (`src/OutputDirMessenger.cc`) — same effect as `Configure()` when nothing is set yet, a no-op when the macro repeats the already-configured path, and a fatal `G4Exception` when it differs (raised by the messenger, not by `OutputDir` itself, which stays pure/testable). It also holds a separate filename prefix (`SetPrefix()`/`gPrefix`, distinct from the directory) prepended before the directory join — set by `RunAccumulatorMessenger` around each `/run/dumpDataAndReset` flush, not exposed as its own macro command.
- `src/DetectorConstruction.cc`: homogeneous water-box geometry; owns the `DnaChemistryWorld` (its boundary sizes the world box).
- `src/PhysicsList.cc`: simplified UHDR-style modular list — holds `G4EmDNAPhysics` + `DnaChemistryList` and drives their `ConstructParticle()`/`ConstructProcess()` directly (no string dispatch, no `RegisterPhysics`). Change the EM-DNA physics option by editing the constructor. Sets SBS as the chemistry time-step model (the only one `DnaChemistryList` supports).
- `src/DnaChemistryList.cc`: project chemical stage (`G4VUserChemistryList` + `G4VPhysicsConstructor`) — molecule set, water dissociation, reaction table, time-step model. Replaces macro `/chem/species` and `/chem/reaction/add`. Hard-codes SBS as the only chemistry time-step model (IRT and IRT_syn are not supported). Pure-water + O2-derived reaction chemistry lives in `PureWaterReactions.cc`; the pH-driven acid-base buffer network (`H3Op(B)`/`OHm(B)`, registered as per-molecule `G4DNAScavengerProcess`) and the full O2⁻/HO2/HO2⁻/O⁻/O3⁻ network are baseline/unconditional — this chemistry can produce O2 from pure water radiolysis on its own (see `docs/adr/0001-baseline-acid-base-buffer.md`). Only an exogenous dissolved-O2 supply mechanism remains deferred to future work — `/chem/env/O2` currently has no effect on the chemistry. Also owns `/chem/reaction/timeBinsFixed` / `timeBinsList` (`ApplyReactionTimeBinning()`, applied from `RunAction::BeginOfRunAction` once the scheduler end time is final) and `/chem/reaction/dump`.
- `src/ReactionCounter.cc`: per-time-bin bimolecular reaction-firing counts. Kernel/DLL-free by design (see file header) — `ConfigureBinEdges`/`ParseBinEdgesList` are covered by `test/ReactionCounterTest.cc`; do not call `G4UnitDefinition::GetValueOf()` from here or its test (observed to corrupt memory when the Debug test binary links a differently-built Geant4 install — use the local `TimeUnitValue` table instead).
- `src/PureWaterReactions.cc`: portable, project-agnostic reaction-table builder (`PureWaterReactions::BuildPureWaterReactions`) — the 9 base pure-water reactions plus the full O2-derived second-order network from the Geant4-DNA UHDR example. No `dnachem-min`-specific includes; copy-paste portable to another project.
- `src/PhysicsInteractionCounter.cc`: portable, project-agnostic string-frequency counter (`Record`/`Merge`/`Clear`/`WriteAscii`/`WriteCsv`, stream-only I/O, no dnachem-min-specific includes) — copy-paste portable to another Geant4-DNA project, same convention as `PureWaterReactions.cc`.
- `src/RunAccumulator.cc`: process-wide accumulator for energy deposit and the two counters (`ReactionCounter`, `PhysicsInteractionCounter`), persisting across `/run/beamOn` calls — pure logic, no Geant4-kernel dependency, covered by `test/RunAccumulatorTest.cc`. Fed once per run from `RunAction::EndOfRunAction`; species yields don't need this (the `ScoreSpecies` scorer is itself persistent).
- `src/RunAccumulatorMessenger.cc`: exposes `/run/dumpDataAndReset [prefix]` (`Idle` state) — writes everything accumulated since the last dump (species via a direct `ScoreSpecies` lookup, the rest via `RunAccumulator`), then resets it; `prefix` is prepended literally to every output filename via `OutputDir::SetPrefix`. Refuses to reuse a prefix already used earlier in the process (fatal `G4Exception`). Also exposes `FlushIfPending()`, called once from `sim.cc` right before the run manager is destroyed, as a safety net (fixed prefix `EndOfRun_`) for data never manually flushed.
- `src/SteppingAction.cc`: per-step physical-interaction counting — records `G4Step::GetPostStepPoint()->GetProcessDefinedStep()->GetProcessName()` into a `PhysicsInteractionCounter` whenever the name contains `"G4DNA"` (discrete G4DNA physics processes only, excludes `Transportation`).
- `src/DnaChemistryWorld.cc`: `G4VChemistryWorld` subclass — diffusion boundary + bulk solvent composition (water, H3O+/OH- from pH, optional dissolved O2). Messenger: `/chem/env/pH`, `/chem/env/O2` (both PreInit).
- `src/PrimaryGeneratorAction.cc`: electron source configuration.
- `src/TimeStepAction.cc`: chemistry time stepping.
- `src/ScoreSpecies.cc`: species-yield and G-value scoring, including HDF5 output.
- `macro/*.in`: runtime beam and chemistry configuration. Prefer macro changes for species and reaction studies rather than hard-coding them.
- `src/DnaLogger.cc` and `src/DnaLoggerMessenger.cc`: project-defined logging framework for console output and diagnostics.
- `src/ChemUtils.cc`: utility functions for chemistry species and reactions.

## Macro and Logging

Common macro controls include `/run/initialize`, `/run/outputDir <path>` (macro-file counterpart to `--dir`, see above), `/run/dumpDataAndReset [prefix]` / `/run/dumpDataAndResetToDir <subdir>` (dump-and-reset accumulated output, into filename-prefixed files or a subfolder, see above), `/gun/particle e-`, `/gun/energy`, `/process/chem/TimeStepModel` (`SBS` only — `DnaChemistryList` hard-codes SBS regardless of this command), `/chem/env/O2 <percent>` (currently a no-op for chemistry — the O2/acid-base network is always active regardless; reserved for a future dissolved-O2 supply mechanism), `/chem/env/pH <double>` (still active — drives the bulk H3O+(B)/OH-(B) buffer concentration used by the always-on acid-base network), `/chem/reaction/timeBinsFixed <width> <unit>` / `/chem/reaction/timeBinsList <e1> ... <eN> <unit>` (reaction-count time binning, see above), and `/run/beamOn`. Species and reactions are defined in `src/DnaChemistryList.cc`/`src/PureWaterReactions.cc`, not via `/chem/species` / `/chem/reaction/add` — do not re-add those to macros (`/chem/reaction/UI` resets the shared reaction table and wipes the class-built one). Example macros: `beam.in` (pure water), `beam_02.in` (SBS variant), `beam_o2.in` (sets `/chem/env/O2 21`, currently inert; pH = 7).

Use the project-defined `DnaLogger` for application logging. Set its level in a macro with:

```text
/dnaLogger/verbose Quiet|Error|Warning|Info|Debug|Trace
```

The logger is implemented in `src/DnaLogger.cc` and exposed to Geant4 commands by `src/DnaLoggerMessenger.cc`. Use `/dnaLogger/verbose Debug` for detailed diagnostics; do not add ad hoc console logging where `DnaLogger` is appropriate.

## Testing

Unit tests (`test/*Test.cc`, plain `assert` + CTest) must be built and run from `build-ninja/` (Debug); see `.claude/geant4-instructions.md` section 5 for the `NDEBUG` and Debug-CRT-dialog pitfalls.

When testing `sim.exe`, use a 10 keV electron gun and `/run/beamOn 2`. The chemistry time limit defaults to 1 µs, set by `G4Scheduler::Instance()->SetEndTime(1. * microsecond)` in `src/ActionInitialization.cc::Build()`, which runs on `/run/initialize`. To override it in a macro, issue `/scheduler/endTime <value> <unit>` *after* `/run/initialize` — a command issued before that point gets overwritten by `Build()`'s hardcoded call.

## Planning
Before any non-trivial change, enter plan mode and write the plan to `.claude/plans/`.
Read any existing relevant plan first — don't silently overwrite an unrelated one.
Wait for explicit approval before executing.

## Agent skills

### Issue tracker

Issues and specs live as local markdown files under `.scratch/<feature-slug>/`. See `docs/agents/issue-tracker.md`.

### Domain docs

Single-context layout: `CONTEXT.md` + `docs/adr/` at the repo root. See `docs/agents/domain.md`.
