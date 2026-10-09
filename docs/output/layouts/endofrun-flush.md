# Layout: `EndOfRun_` flush

A macro with no `/run/dumpDataAndReset` or `/run/dumpDataAndResetToDir` still writes its data: `sim` flushes once at exit with the fixed prefix `EndOfRun_`, into the output directory (here `--dir`; without it, the current directory). The prefix is glued on like any other. File meaning: [../README.md](../README.md).

## Default case

```bash
./sim <macro>.in --dir <outdir>
```

Macro lines that differ per case (the rest is `/gun/particle e-`, `/gun/energy 10 keV`):

```text
/scheduler/endTime 1 ms            (after /run/initialize)
/run/beamOn 2
(no dump command)
```

```text
<outdir>/
  EndOfRun_Manifest.json
  EndOfRun_Species.Txt
  EndOfRun_Species_nt_species.csv
  EndOfRun_Reactions.Txt
  EndOfRun_Reactions_nt_reactions.csv
  EndOfRun_ReactionsMetadata.csv
  EndOfRun_PhysicsInteractions.Txt
  EndOfRun_PhysicsInteractions.csv
  EndOfRun_SpeciesMeso.Txt
  EndOfRun_SpeciesMeso.csv
  EndOfRun_PreChemical_run0_event0.txt
  EndOfRun_...
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
(no dump command)
```

```text
<outdir>/
  EndOfRun_Manifest.json
  EndOfRun_Species.Txt
  EndOfRun_Species_nt_species.csv
  EndOfRun_Reactions.Txt
  EndOfRun_Reactions_nt_reactions.csv
  EndOfRun_ReactionsMetadata.csv
  EndOfRun_PhysicsInteractions.Txt
  EndOfRun_PhysicsInteractions.csv
  EndOfRun_SpeciesMeso.Txt
  EndOfRun_SpeciesMeso.csv
  EndOfRun_SpeciesMesoSpatial.h5
  EndOfRun_PreChemical_run0_event0.txt
  EndOfRun_...
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
(no dump command)
```

```text
<outdir>/
  EndOfRun_Manifest.json
  EndOfRun_Species.Txt
  EndOfRun_Species_nt_species.csv
  EndOfRun_Reactions.Txt
  EndOfRun_Reactions_nt_reactions.csv
  EndOfRun_ReactionsMetadata.csv
  EndOfRun_PhysicsInteractions.Txt
  EndOfRun_PhysicsInteractions.csv
  EndOfRun_SpeciesMeso.Txt
  EndOfRun_SpeciesMeso.csv
  EndOfRun_PreChemical_run0_event0.txt
  EndOfRun_...
  .pending_prechem/            (empty)
```

`SpeciesMeso.Txt` and `SpeciesMeso.csv` hold only a header line here (51 B and 23 B), because the chemistry stops before the mesoscopic stage. Nothing else changes; see the [edge case](../README.md#edge-case-end-time-at-or-before-the-hand-over-time).

