---
name: sim-output
description: Use when running sim.exe, writing macros that dump output (/run/dumpDataAndReset, /run/dumpDataAndResetToDir, /run/outputDir, --dir), analyzing its output files, or changing output/scoring code (RunAccumulator, OutputDir, counters).
---

# sim.exe output files and dump commands

Species/reaction/energy/physical-interaction data accumulates across `/run/beamOn`
calls rather than being written and reset after every run — species yields
accumulate automatically (the `ScoreSpecies` scorer is itself a persistent,
SD-registered object), while energy deposit and the two counters accumulate via
`RunAccumulator` (`src/scoring/RunAccumulator.cc`), fed once per run from
`RunAction::EndOfRunAction`. Nothing is written to disk until a macro issues
`/run/dumpDataAndReset [prefix]` (`Idle` state, i.e. between `beamOn` calls),
which writes everything accumulated since the last dump (or program start) and
resets all counters to empty/zero:

- `Species.Txt` (human-readable species yields vs. time) and the CSV ntuple
  `Species_nt_species.csv` (aggregate sumG/sumG2 per species/time); a second
  ntuple, `Species_nt_species_all.csv` (same, per event), is only written when
  `ScoreSpecies` is compiled with `_ScoreSpecies_FOR_ALL_EVENTS` (off by
  default; see `header/scoring/ScoreSpecies.hh`). In MT mode the per-event
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
- `Manifest.json`: one per dump, describing what produced the data beside it
  (`ManifestWriter` serialises, `RunManifest` collects; `schemaVersion: 1`,
  units in the keys). Top level: `timestamp`, `geant4Version`, `macro`,
  `chemistry`, `scavengers` (`species`, `molarity_M`), `pH`,
  `chemistryEndTime_ns`, `runMode` (`Serial`/`MT`), `threads`,
  `outputDirAsConfigured`/`outputDirAbsolute`, `prefix`, `subdir`,
  `totalEvents`, `totalEnergyDeposit_eV` (sums over `runs`; this replaces the
  old `EnergyDeposit.Txt`), `files` (the data files this dump wrote, relative
  to the manifest's folder, manifest excluded) and `runs[]`: one entry per
  `/run/beamOn` folded into the dump with `run`, `events`, `particle`,
  `beamEnergy_keV`, `position_um`, `direction`, `energyDeposit_eV` and `seed`.
  The beam is what the gun actually had on the first event of that run, so
  several `/gun/energy` values between dumps show up as separate `runs[]`
  entries. `seed` is the random engine's seed as configured when the run
  started (it follows `/random/setSeeds`); output is still not bit-reproducible
  from it. It is written on every dump, including the `EndOfRun_` flush, and
  carries the same prefix or subfolder as the data files
  (`EndOfRun_Manifest.json`, `run01/Manifest.json`). Read totals from here;
  there is no `EnergyDeposit.Txt` any more.
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
