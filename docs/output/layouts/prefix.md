# Layout: filename prefix

`/run/dumpDataAndReset <prefix>` writes into the output directory and glues the prefix in front of every file name, without a separator (`pfx_` below includes its underscore). The staging folders stay unprefixed. A prefix can be used once per process. File meaning: [../README.md](../README.md).

## Default case

```bash
./sim <macro>.in --dir <outdir>
```

Macro lines that differ per case (the rest is `/gun/particle e-`, `/gun/energy 10 keV`):

```text
/scheduler/endTime 1 ms            (after /run/initialize)
/run/beamOn 2
/run/dumpDataAndReset pfx_
```

```text
<outdir>/
  pfx_Manifest.json
  pfx_Species.Txt
  pfx_Species_nt_species.csv
  pfx_Reactions.Txt
  pfx_Reactions_nt_reactions.csv
  pfx_ReactionsMetadata.csv
  pfx_PhysicsInteractions.Txt
  pfx_PhysicsInteractions.csv
  pfx_SpeciesMeso.Txt
  pfx_SpeciesMeso.csv
  pfx_PreChemical_run0_event0.txt
  pfx_...
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
/run/dumpDataAndReset pfx_
```

```text
<outdir>/
  pfx_Manifest.json
  pfx_Species.Txt
  pfx_Species_nt_species.csv
  pfx_Reactions.Txt
  pfx_Reactions_nt_reactions.csv
  pfx_ReactionsMetadata.csv
  pfx_PhysicsInteractions.Txt
  pfx_PhysicsInteractions.csv
  pfx_SpeciesMeso.Txt
  pfx_SpeciesMeso.csv
  pfx_SpeciesMesoSpatial.h5
  pfx_PreChemical_run0_event0.txt
  pfx_...
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
/run/dumpDataAndReset pfx_
```

```text
<outdir>/
  pfx_Manifest.json
  pfx_Species.Txt
  pfx_Species_nt_species.csv
  pfx_Reactions.Txt
  pfx_Reactions_nt_reactions.csv
  pfx_ReactionsMetadata.csv
  pfx_PhysicsInteractions.Txt
  pfx_PhysicsInteractions.csv
  pfx_SpeciesMeso.Txt
  pfx_SpeciesMeso.csv
  pfx_PreChemical_run0_event0.txt
  pfx_...
  .pending_prechem/            (empty)
```

`SpeciesMeso.Txt` and `SpeciesMeso.csv` hold only a header line here (51 B and 23 B), because the chemistry stops before the mesoscopic stage. Nothing else changes; see the [edge case](../README.md#edge-case-end-time-at-or-before-the-hand-over-time).

