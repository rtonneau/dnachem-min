Status: done

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

## Addendum — done; one acceptance-check line was wrong, unrelated to this ticket

Implemented and verified against a real build+run (`beam_02.in`, 100 keV,
2 events, ~687s real time — primaries now run to full energy deposition per
the project's current `primaryKiller` defaults, unrelated to this ticket).

- `fOutputType` default changed to `"csv"`, `OpenFile("Species.root")` changed
  to `OpenFile("Species")` (`src/ScoreSpecies.cc`). Also updated the
  `Species.root`-naming comments/log message in `src/RunAction.cc` and
  `CLAUDE.md`'s Build and Run section.
- Verified: `Species_nt_species.csv` produced with the exact documented
  format (`#class tools::wcsv::ntuple` first line, `#column string
  speciesName` header line present, 7 comma-separated columns in the
  documented order). No `Species.root`. No `_bis`-suffixed file (confirmed
  `beam_02.in` has exactly one `/run/beamOn`).
- **Correction to this ticket's acceptance check**: `Species_nt_species_all.csv`
  is **not** produced, and this is not a bug in this ticket's change.
  `header/ScoreSpecies.hh:23` has `_ScoreSpecies_FOR_ALL_EVENTS` commented
  out, which gates the entire `species_all` ntuple code block
  (`src/ScoreSpecies.cc:362-406`, both creation and fill) off by default —
  true regardless of ROOT vs CSV output format, and predates this ticket.
  Enabling that macro is a separate, unrelated feature toggle (with its own
  memory/perf cost for per-event tracking) that this ticket does not touch.
  If the `species_all` per-event ntuple is wanted, that's a new, separate
  ticket.
