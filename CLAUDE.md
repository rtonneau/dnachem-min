# dnachem-min

## Purpose and Scope

Minimal Geant4-DNA example for studying water radiolysis from electron beams. It provides the common physics, chemistry, and scoring basis for conventional and ultra-high-dose-rate (UHDR) irradiation studies.

UHDR is a use case, not a separate implementation: do not add UHDR-specific pulse structure, dose-rate effects, delivery models, or dedicated physics/chemistry models unless the project scope changes.

The geometry is one homogeneous water box. Do not introduce voxelization or G4Vox-based geometry.

## Build and Run

Requirements: Geant4 11.0+ with DNA models, CMake 3.16+, a C++20 compiler, and HDF5 with C++ support.

Building always requires the MSVC x64 dev environment loaded in the shell first — a plain shell fails on the compile step with missing STL headers (e.g. `cstddef`, `complex`) even though `cmake` configures without error. Load it before the build commands below, e.g. from PowerShell:

```powershell
$vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
cmd /c "`"$vsPath\VC\Auxiliary\Build\vcvars64.bat`" && <build commands>"
```

Build outside the source tree using `RelWithDebInfo`:

```bash
cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
cmake --build . --config RelWithDebInfo
```

Run from `build/`; the build copies `macro/` there. `sim.cc` prepends `macro/` to the argument itself, so pass the filename only:

```bash
./sim beam.in      # pure-water radiolysis
./sim beam_o2.in   # with dissolved-O2 scavenger
```

Each run writes `Species.Txt` (human-readable species yields vs. time) and two CSV
ntuples, `Species_nt_species.csv` (aggregate sumG/sumG2 per species/time) and
`Species_nt_species_all.csv` (same, per event); in MT mode the per-event
pre-chemical dumps are `output_event_t<thread>_e<event>.txt`.

## Key Files

- `sim.cc`: application setup. Runs multithreaded by default (`G4RunManagerType::MT`, batch default 4 threads; `/run/numberOfThreads N` overrides before `/run/initialize`; flip to `Serial` in one line for reproducible runs). Set `useGUI` to `true` only for interactive GUI runs.
- `src/DetectorConstruction.cc`: homogeneous water-box geometry; owns the `DnaChemistryWorld` (its boundary sizes the world box).
- `src/PhysicsList.cc`: simplified UHDR-style modular list — holds `G4EmDNAPhysics` + `DnaChemistryList` and drives their `ConstructParticle()`/`ConstructProcess()` directly (no string dispatch, no `RegisterPhysics`). Change the EM-DNA physics option by editing the constructor. Sets SBS as the chemistry time-step model (the only one `DnaChemistryList` supports).
- `src/DnaChemistryList.cc`: project chemical stage (`G4VUserChemistryList` + `G4VPhysicsConstructor`) — molecule set, water dissociation, reaction table, time-step model. Replaces macro `/chem/species` and `/chem/reaction/add`. Hard-codes SBS as the only chemistry time-step model (IRT and IRT_syn are not supported). Pure-water + O2-derived reaction chemistry lives in `PureWaterReactions.cc`; the pH-driven acid-base buffer network (`H3Op(B)`/`OHm(B)`, registered as per-molecule `G4DNAScavengerProcess`) and the full O2⁻/HO2/HO2⁻/O⁻/O3⁻ network are baseline/unconditional — this chemistry can produce O2 from pure water radiolysis on its own (see `docs/adr/0001-baseline-acid-base-buffer.md`). Only an exogenous dissolved-O2 supply mechanism remains deferred to future work — `/chem/env/O2` currently has no effect on the chemistry.
- `src/PureWaterReactions.cc`: portable, project-agnostic reaction-table builder (`PureWaterReactions::BuildPureWaterReactions`) — the 9 base pure-water reactions plus the full O2-derived second-order network from the Geant4-DNA UHDR example. No `dnachem-min`-specific includes; copy-paste portable to another project.
- `src/DnaChemistryWorld.cc`: `G4VChemistryWorld` subclass — diffusion boundary + bulk solvent composition (water, H3O+/OH- from pH, optional dissolved O2). Messenger: `/chem/env/pH`, `/chem/env/O2` (both PreInit).
- `src/PrimaryGeneratorAction.cc`: electron source configuration.
- `src/TimeStepAction.cc`: chemistry time stepping.
- `src/ScoreSpecies.cc`: species-yield and G-value scoring, including HDF5 output.
- `macro/*.in`: runtime beam and chemistry configuration. Prefer macro changes for species and reaction studies rather than hard-coding them.
- `src/DnaLogger.cc` and `src/DnaLoggerMessenger.cc`: project-defined logging framework for console output and diagnostics.
- `src/ChemUtils.cc`: utility functions for chemistry species and reactions.

## Macro and Logging

Common macro controls include `/run/initialize`, `/gun/particle e-`, `/gun/energy`, `/process/chem/TimeStepModel` (`SBS` only — `DnaChemistryList` hard-codes SBS regardless of this command), `/chem/env/O2 <percent>` (currently a no-op for chemistry — the O2/acid-base network is always active regardless; reserved for a future dissolved-O2 supply mechanism), `/chem/env/pH <double>` (still active — drives the bulk H3O+(B)/OH-(B) buffer concentration used by the always-on acid-base network), and `/run/beamOn`. Species and reactions are defined in `src/DnaChemistryList.cc`/`src/PureWaterReactions.cc`, not via `/chem/species` / `/chem/reaction/add` — do not re-add those to macros (`/chem/reaction/UI` resets the shared reaction table and wipes the class-built one). Example macros: `beam.in` (pure water), `beam_02.in` (SBS variant), `beam_o2.in` (sets `/chem/env/O2 21`, currently inert; pH = 7).

Use the project-defined `DnaLogger` for application logging. Set its level in a macro with:

```text
/dnaLogger/verbose Quiet|Error|Warning|Info|Debug|Trace
```

The logger is implemented in `src/DnaLogger.cc` and exposed to Geant4 commands by `src/DnaLoggerMessenger.cc`. Use `/dnaLogger/verbose Debug` for detailed diagnostics; do not add ad hoc console logging where `DnaLogger` is appropriate.

## Planning
Before any non-trivial change, enter plan mode and write the plan to `.claude/plans/`.
Read any existing relevant plan first — don't silently overwrite an unrelated one.
Wait for explicit approval before executing.

## Agent skills

### Issue tracker

Issues and specs live as local markdown files under `.scratch/<feature-slug>/`. See `docs/agents/issue-tracker.md`.

### Domain docs

Single-context layout: `CONTEXT.md` + `docs/adr/` at the repo root. See `docs/agents/domain.md`.
