---
status: accepted
---

# One JSON manifest per dump, replacing EnergyDeposit.Txt

Every **Dump** writes a `Manifest.json` (filename and location resolved like every other output file: prefix or subfolder applied) describing what produced the data next to it: `schemaVersion`, the process-wide settings (Chemistry, scavengers, pH, chemistry end time, thread count, Geant4 version), and a `runs[]` array with one entry per `/run/beamOn` folded into the dump (beam particle, energy, position, direction, events, that run's energy deposit, seed), plus totals and the list of data files written. It is written on every dump, including the automatic `EndOfRun_` flush; there is no opt-out switch.

The unit is the dump, not the `/run/beamOn`, because data files only exist at dump time and a dump can span several runs with different beams; a per-`beamOn` manifest would have no data beside it. The beam is captured from the real gun state on a worker thread, carried in the run record, merged in `Run::Merge` and stored per run in `RunAccumulator`, rather than mirrored from `/gun/*` commands on the master, which could drift from the actual gun.

JSON is written by a small, portable, stream-only `ManifestWriter` that serialises a plain-data struct (no dependency on other project classes, testable without the Geant4 kernel); a thin Geant4-facing collector fills the struct. Numbers carry their unit in the key (`beamEnergy_keV`, `position_um`, `energyDeposit_eV`); directories are stored both as configured and as absolute paths.

`EnergyDeposit.Txt` is removed: the manifest carries `totalEnergyDeposit_eV`, and keeping a second, human-readable copy was rejected. Anything that read `EnergyDeposit.Txt` must read `Manifest.json` instead.

**Consequences**: the manifest records the seed as configured, but output is still not bit-reproducible (see the project notes on non-reproducible chemistry stepping), so it documents a run rather than guaranteeing a replay. A failed manifest write is a warning, not fatal, since the data files are already written. Changing the meaning of an existing field bumps `schemaVersion`; adding fields does not.
