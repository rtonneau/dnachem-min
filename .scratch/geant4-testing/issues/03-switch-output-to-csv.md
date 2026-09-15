Status: ready-for-agent

# Switch ScoreSpecies analysis-manager output from ROOT to CSV

Spec: `.scratch/geant4-testing/spec.md` (Implementation Decisions, "Output
format"). Depends on: 02.

## Context — two things verified empirically this session, don't skip them

1. **Changing `fOutputType` alone does nothing.** `ScoreSpecies::WriteWithAnalysisManager`
   calls `analysisManager->OpenFile("Species.root")` with a hardcoded `.root`
   extension. Tested: setting `fOutputType("csv")` in the constructor but
   leaving `OpenFile("Species.root")` unchanged still produced `Species.root`
   — the literal extension in the `OpenFile` argument determines the actual
   format, not (only) `SetDefaultFileType`. **Both** the `fOutputType` field
   default and the `OpenFile(...)` call's filename argument must change (drop
   the `.root` extension, e.g. `OpenFile("Species")`).

2. **Multiple `/run/beamOn` calls in one process produce a second,
   differently-named file under CSV.** `beam.in` calls `/run/beamOn` twice
   (4 events, then 2). Tested with CSV output: the second `OutputAndClear()`
   call produced `Species_nt_species_bis.csv` instead of overwriting/appending
   to the first file — a behavior ROOT output didn't exhibit the same way.
   **Do not use `beam.in` to verify this ticket** — use `beam_02.in`, which
   has exactly one `/run/beamOn` call, for a clean, single, predictably-named
   output. This `_bis` behavior should be called out in this ticket's PR/commit
   description so ticket 05 designs its dedicated test macro with exactly one
   `/run/beamOn`.

Verified exact CSV format (from `Species_nt_species.csv` when produced): 11
header lines prefixed with `#` (`#class`, `#title`, `#separator`,
`#vector_separator`, then one `#column <type> <name>` line per column), then
data rows. Columns for the `species` ntuple, in order: `speciesID` (int),
`number` (int), `nEvent` (int), `speciesName` (string), `time` (double),
`sumG` (double), `sumG2` (double). The `species_all` ntuple has the same
columns plus `eventID` (int). Separator is `,` (comma), vector separator `;`.

## Task

1. In `src/ScoreSpecies.cc`, change the `fOutputType` constructor default from
   `"root"` to `"csv"`.
2. Change `analysisManager->OpenFile("Species.root")` to
   `analysisManager->OpenFile("Species")` (no extension — let the analysis
   manager apply the extension matching `fOutputType`).
3. Update any comments/docs referencing `Species.root` as the output filename
   (e.g. `CLAUDE.md`'s Build and Run section, `RunAction.cc`'s log message if
   it names the file literally) to describe the new CSV filenames instead.

## Acceptance check

```
cd build
cmake --build . --config RelwithDebInfo
./sim.exe beam_02.in
dir Species*
```
Expected: `Species_nt_species.csv`, `Species_nt_species_all.csv`, and
`Species.Txt` are produced; **no** `Species.root` file; **no** `_bis`-suffixed
file (since `beam_02.in` has exactly one `/run/beamOn`).

```
type Species_nt_species.csv
```
Expected first line: `#class tools::wcsv::ntuple`. Expected a
`#column string speciesName` line among the header lines. Expected data rows
starting after the header block, comma-separated, matching the 7-column
order listed above.
