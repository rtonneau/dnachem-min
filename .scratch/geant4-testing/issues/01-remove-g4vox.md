Status: ready-for-agent

# Remove the G4Vox dependency completely

Spec: `.scratch/geant4-testing/spec.md` (Implementation Decisions, "G4Vox removal").

## Context

`CLAUDE.md` already says not to introduce voxelization or G4Vox-based
geometry, implying it's unused. Verified this session: `grep`-ing for
`G4Vox`/`Voxel` across `src/`/`header/` found exactly one match,
`src/DetectorConstruction.cc`'s file-header doc comment ("`brief
Implementation of the DetectorConstruction class for Voxel use`") — a stale
comment, not an actual API call. No other source file references G4Vox at
all. This is a prerequisite step for the rest of this ticket sequence (a
lighter local build makes iterating on the remaining tickets faster).

## Task

1. Remove `find_package(G4Vox REQUIRED)` and the `G4Vox::G4Vox` entry in
   `target_link_libraries(...)` from `CMakeLists.txt`.
2. Fix the stale doc comment in `src/DetectorConstruction.cc` to describe its
   actual purpose (homogeneous water-box geometry), not voxel use.
3. Before removing, re-verify nothing else references G4Vox (a second grep
   pass, since another agent may have added references between this ticket
   being written and being picked up).

## Acceptance check

```
grep -ril "g4vox" CMakeLists.txt src header
```
Expected: no output (zero matches).

```
cd build
cmake -DCMAKE_BUILD_TYPE=RelwithDebInfo ..
cmake --build . --config RelwithDebInfo
```
Expected: exit code 0, `build/sim.exe` produced, no G4Vox-related linker or
include errors.

```
./sim.exe beam.in
```
Expected: runs to completion, no fatal `G4Exception`, `Species.Txt` and
`Species.root` produced (same as before this change — geometry/output
behavior is unaffected by removing an unused dependency).
