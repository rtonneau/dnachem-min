# dnachem-min

## Purpose and Scope

Minimal Geant4-DNA example for studying water radiolysis from electron beams. It provides the common physics, chemistry, and scoring basis for conventional and ultra-high-dose-rate (UHDR) irradiation studies.

UHDR is a use case, not a separate implementation: do not add UHDR-specific pulse structure, dose-rate effects, delivery models, or dedicated physics/chemistry models unless the project scope changes.

The geometry is one homogeneous water box. Do not introduce voxelization or G4Vox-based geometry.

## Build and Run

Requirements: Geant4 11.0+ with DNA models, CMake 3.16+, a C++20 compiler, and HDF5 with C++ support.

Build outside the source tree using `RelwithDebInfo`:

```bash
cd build
cmake -DCMAKE_BUILD_TYPE=RelwithDebInfo ..
cmake --build . --config RelwithDebInfo
```

Run from `build/`; the build copies `macro/` there:

```bash
./sim macro/beam.in      # pure-water radiolysis
./sim macro/beam_o2.in   # with dissolved-O2 scavenger
```

Each run writes `Species.Txt` (human-readable species yields vs. time) and `Species.root`;
in MT mode the per-event pre-chemical dumps are `output_event_t<thread>_e<event>.txt`.

## Key Files

- `sim.cc`: application setup. Runs multithreaded by default (`G4RunManagerType::MT`, batch default 4 threads; `/run/numberOfThreads N` overrides before `/run/initialize`; flip to `Serial` in one line for reproducible runs). Set `useGUI` to `true` only for interactive GUI runs.
- `src/DetectorConstruction.cc`: homogeneous water-box geometry; owns the `DnaChemistryWorld` (its boundary sizes the world box).
- `src/PhysicsList.cc`: simplified UHDR-style modular list — holds `G4EmDNAPhysics` + `DnaChemistryList` and drives their `ConstructParticle()`/`ConstructProcess()` directly (no string dispatch, no `RegisterPhysics`). Change the EM-DNA physics option by editing the constructor. Sets SBS as the default chemistry time-step model.
- `src/DnaChemistryList.cc`: project chemical stage (`G4VUserChemistryList` + `G4VPhysicsConstructor`) — molecule set, water dissociation, reaction table, time-step model. Replaces macro `/chem/species` and `/chem/reaction/add`. Rejects the IRT time-step model (SBS or IRT_syn only). Adds the UHDR O2 sub-system (bulk-O2 scavenging + O2⁻/HO2 chemistry + per-molecule `G4DNAScavengerProcess`) when O2 is enabled.
- `src/DnaChemistryWorld.cc`: `G4VChemistryWorld` subclass — diffusion boundary + bulk solvent composition (water, H3O+/OH- from pH, optional dissolved O2). Messenger: `/chem/env/pH`, `/chem/env/O2` (both PreInit).
- `src/PrimaryGeneratorAction.cc`: electron source configuration.
- `src/TimeStepAction.cc`: chemistry time stepping.
- `src/ScoreSpecies.cc`: species-yield and G-value scoring, including HDF5 output.
- `macro/*.in`: runtime beam and chemistry configuration. Prefer macro changes for species and reaction studies rather than hard-coding them.
- `src/DnaLogger.cc` and `src/DnaLoggerMessenger.cc`: project-defined logging framework for console output and diagnostics.
- `src/ChemUtils.cc`: utility functions for chemistry species and reactions.

## Macro and Logging

Common macro controls include `/run/initialize`, `/gun/particle e-`, `/gun/energy`, `/process/chem/TimeStepModel` (`SBS` default, or `IRT_syn`; **not** `IRT`), `/chem/env/O2 <percent>` (optional dissolved-O2 scavenger; 0 = anoxic), `/chem/env/pH <double>`, and `/run/beamOn`. Species and reactions are defined in `src/DnaChemistryList.cc`, not via `/chem/species` / `/chem/reaction/add` — do not re-add those to macros (`/chem/reaction/UI` resets the shared reaction table and wipes the class-built one). Example macros: `beam.in` (pure water), `beam_02.in` (SBS variant), `beam_o2.in` (O2 = 21 %).

Use the project-defined `DnaLogger` for application logging. Set its level in a macro with:

```text
/dnaLogger/verbose Quiet|Error|Warning|Info|Debug|Trace
```

The logger is implemented in `src/DnaLogger.cc` and exposed to Geant4 commands by `src/DnaLoggerMessenger.cc`. Use `/dnaLogger/verbose Debug` for detailed diagnostics; do not add ad hoc console logging where `DnaLogger` is appropriate.

## Planning
Before any non-trivial change, enter plan mode and write the plan to `.claude/plans/`.
Read any existing relevant plan first — don't silently overwrite an unrelated one.
Wait for explicit approval before executing.
