# Layout: staging folders

Two hidden folders in the output directory hold files between the end of an event and the next dump: `.pending_prechem/` (one `PreChemical_run<R>_event<E>.txt` per event) and `.pending_meso_spatial/` (`SpeciesMesoSpatial.h5`, only with `/chem/meso/spatialOutput true`). Each dump moves the files out and leaves both folders in place, empty. File meaning: [../README.md](../README.md).

Each tree below is the listing taken by `/control/shell dir /s /b <outdir>` right after `/run/beamOn 2` (before the dump), then the tree after `/run/dumpDataAndReset`.

## Default case

```bash
./sim <macro>.in --dir <outdir>
```

Macro lines that differ per case (the rest is `/gun/particle e-`, `/gun/energy 10 keV`):

```text
/scheduler/endTime 1 ms            (after /run/initialize)
/run/beamOn 2
/control/shell dir /s /b <outdir>
/run/dumpDataAndReset
```

After `/run/beamOn 2`, before the dump:

```text
<outdir>/
  .pending_prechem/
    PreChemical_run0_event0.txt
    PreChemical_run0_event1.txt
```

After the dump:

```text
<outdir>/
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
./sim <macro>.in --dir <outdir>
```

Macro lines that differ per case (the rest is `/gun/particle e-`, `/gun/energy 10 keV`):

```text
/chem/meso/spatialOutput true     (before /run/initialize)
/scheduler/endTime 1 ms            (after /run/initialize)
/run/beamOn 2
/control/shell dir /s /b <outdir>
/run/dumpDataAndReset
```

After `/run/beamOn 2`, before the dump:

```text
<outdir>/
  .pending_prechem/
    PreChemical_run0_event0.txt
    PreChemical_run0_event1.txt
  .pending_meso_spatial/
    SpeciesMesoSpatial.h5
```

After the dump:

```text
<outdir>/
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
./sim <macro>.in --dir <outdir>
```

Macro lines that differ per case (the rest is `/gun/particle e-`, `/gun/energy 10 keV`):

```text
/scheduler/endTime 3 ns            (after /run/initialize)
/run/beamOn 2
/control/shell dir /s /b <outdir>
/run/dumpDataAndReset
```

After `/run/beamOn 2`, before the dump:

```text
<outdir>/
  .pending_prechem/
    PreChemical_run0_event0.txt
    PreChemical_run0_event1.txt
```

After the dump:

```text
<outdir>/
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

