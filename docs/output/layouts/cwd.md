# Layout: current directory

No `--dir` and no `/run/outputDir`: output goes to the directory `sim` was started from, with bare file names. One `/run/dumpDataAndReset` writes everything. File meaning: [../README.md](../README.md).

## Default case

```bash
cd <workdir>        # contains macro/
./sim <macro>.in
```

Macro lines that differ per case (the rest is `/gun/particle e-`, `/gun/energy 10 keV`):

```text
/scheduler/endTime 1 ms            (after /run/initialize)
/run/beamOn 2
/run/dumpDataAndReset
```

```text
<workdir>/
  macro/
  Manifest.json
  Species.Txt
  Species_nt_species.csv
  Reactions.Txt
  Reactions_nt_reactions.csv
  ReactionsMetadata.csv
  PhysicsInteractions.Txt
  PhysicsInteractions.csv
  SpeciesMeso.Txt
  SpeciesMeso.csv
  PreChemical_run0_event0.txt
  ...
  .pending_prechem/            (empty)
```

## Spatial output

```bash
cd <workdir>        # contains macro/
./sim <macro>.in
```

Macro lines that differ per case (the rest is `/gun/particle e-`, `/gun/energy 10 keV`):

```text
/chem/meso/spatialOutput true     (before /run/initialize)
/scheduler/endTime 1 ms            (after /run/initialize)
/run/beamOn 2
/run/dumpDataAndReset
```

```text
<workdir>/
  macro/
  Manifest.json
  Species.Txt
  Species_nt_species.csv
  Reactions.Txt
  Reactions_nt_reactions.csv
  ReactionsMetadata.csv
  PhysicsInteractions.Txt
  PhysicsInteractions.csv
  SpeciesMeso.Txt
  SpeciesMeso.csv
  SpeciesMesoSpatial.h5
  PreChemical_run0_event0.txt
  ...
  .pending_prechem/            (empty)
  .pending_meso_spatial/       (empty)
```

## End time at or before the hand-over time

```bash
cd <workdir>        # contains macro/
./sim <macro>.in
```

Macro lines that differ per case (the rest is `/gun/particle e-`, `/gun/energy 10 keV`):

```text
/scheduler/endTime 3 ns            (after /run/initialize)
/run/beamOn 2
/run/dumpDataAndReset
```

```text
<workdir>/
  macro/
  Manifest.json
  Species.Txt
  Species_nt_species.csv
  Reactions.Txt
  Reactions_nt_reactions.csv
  ReactionsMetadata.csv
  PhysicsInteractions.Txt
  PhysicsInteractions.csv
  SpeciesMeso.Txt
  SpeciesMeso.csv
  PreChemical_run0_event0.txt
  ...
  .pending_prechem/            (empty)
```

`SpeciesMeso.Txt` and `SpeciesMeso.csv` hold only a header line here (51 B and 23 B), because the chemistry stops before the mesoscopic stage. Nothing else changes; see the [edge case](../README.md#edge-case-end-time-at-or-before-the-hand-over-time).

