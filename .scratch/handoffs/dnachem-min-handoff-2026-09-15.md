# Handoff: dnachem-min — implement the Geant4-DNA testing tickets

Project: `C:\DEV\GEANT4\SIM\dnachem-min` (Geant4-DNA water-radiolysis
chemistry example). Repo: `git@github.com:rtonneau/dnachem-min.git`, branch
`Geant4-testing` (created via `/branch` from `main`). Next session's focus per
the user: **implement the 7 tickets already written**, in order.

## Start here

1. Read `CLAUDE.md` at the repo root first — it has project-specific rules
   (plan mode required for non-trivial changes, coding conventions, where
   domain docs/issue tracker live). It was updated this session; treat it as
   current.
2. Read `.scratch/geant4-testing/spec.md`, then work through
   `.scratch/geant4-testing/issues/01` through `07` **in numeric order** —
   each has a `Status: ready-for-agent` line, exact context, and an exact
   acceptance check (commands + expected output). Ticket 06 was inserted
   mid-session after a real finding (see below); the original spec only
   anticipated 6 tickets — the spec has an addendum note explaining this.
3. Do not re-derive the design decisions in the spec/tickets — they came out
   of an explicit grilling session with the user (via
   `mattpocock-skills:grilling` + `domain-modeling`) and are final. Follow
   them; only stop and ask if something in a ticket turns out to be
   factually wrong when you check it against the actual code (a few already
   were — see "Corrected findings" below).

## Uncommitted working-tree state — important

**Nothing from this session or the prior chemistry-refactor session has been
git-committed yet.** `git status --short` currently shows:

```
 M CLAUDE.md
 M header/DnaChemistryList.hh
 M macro/beam_o2.in
 M src/DnaChemistryList.cc
 M compile_commands.json          (build-regenerated, not a hand edit)
 M .claude/hooks/hook-posttooluse.log   (pre-existing noise, unrelated)
?? .scratch/                      (spec + 7 tickets)
?? CONTEXT.md
?? docs/                          (docs/adr/0001-*.md, docs/agents/*.md incl. new triage-labels.md)
?? header/PureWaterReactions.hh
?? src/PureWaterReactions.cc
```

The `M`/`??` files under `CLAUDE.md`, `header/`, `src/`, `macro/`,
`CONTEXT.md`, `docs/adr/` are a **completed and verified** chemistry refactor
from the prior session (see `C:\Users\rtonneau\.claude\plans\optimized-strolling-rossum.md`
for the full plan) — built and run successfully (`beam.in`, `beam_o2.in`, no
fatal exceptions, expected species like `Om`/`HO2` confirmed appearing). It
was left uncommitted deliberately (user hadn't asked for a commit). Decide
with the user whether to commit that first, or commit incrementally per
ticket — don't just start editing on top of an unreviewed diff without
checking with them.

## Corrected findings from this session — don't rediscover these

Verified directly against source/by running the app, not assumed (see
`.scratch/geant4-testing/issues/02-*.md` and `06-*.md` for full detail):

- **RNG is already deterministic by default.** Two unseeded runs of
  `beam.in` produced byte-identical `Species.Txt`. The gap isn't "add
  reproducibility," it's "make the existing implicit determinism explicit and
  documented" (ticket 02). `/random/setSeeds <a> <b>` is a standard Geant4 UI
  command, already usable from any macro with zero code changes.
- **`sim.cc` runs `G4RunManagerType::Serial` by default, NOT MT.** Line 35
  hardcodes Serial; the MT line is commented out; the `SetNumberOfThreads(4)`
  call is dead code. This contradicted `CLAUDE.md`'s old "runs MT by default"
  claim — ticket 06 (new, inserted after discovering this) adds a CLI
  thread-count argument to opt into MT at runtime and fixes the doc claim.
- **CSV output format gotchas** (`ScoreSpecies.cc`, relevant to ticket 03):
  changing `fOutputType` alone does nothing — `OpenFile("Species.root")` has
  the `.root` extension hardcoded, and that extension (not
  `SetDefaultFileType`) determines the actual format; must also change to
  `OpenFile("Species")`. Also: multiple `/run/beamOn` calls in one process
  (like `beam.in` has) produce a second, `_bis`-suffixed CSV file instead of
  overwriting — this is why ticket 05's dedicated test macro
  (`macro/test_pure_water.in`, not yet created) must use exactly one
  `/run/beamOn`. Verified exact CSV format: 11 `#`-prefixed metadata lines,
  then comma-separated rows, columns
  `speciesID,number,nEvent,speciesName,time,sumG,sumG2`.

## Build environment gotcha

This sandboxed shell does not inherit the MSVC developer environment. Plain
`cmake --build` fails with `Cannot open include file: 'complex'`/`'vector'`.
Route builds through:

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && cd /d C:\DEV\GEANT4\SIM\dnachem-min\build && ninja sim'
```

(or `cmake --build . --config RelwithDebInfo` inside that same `cmd /c`
wrapper). A configured `build/` directory already exists and builds/runs
successfully as of this session. There is also a `build-ninja/` directory
present — not used or verified this session; unclear if it's stale or an
alternate config. Worth a quick check before assuming either is
authoritative.

## Project conventions to follow

- `CLAUDE.md`: "Before any non-trivial change, enter plan mode and write the
  plan to `.claude/plans/`... Wait for explicit approval before executing."
  This was followed for the chemistry refactor (see the plan path above);
  follow it again for ticket implementation unless the user says otherwise.
- `docs/agents/issue-tracker.md`: local-markdown tracker convention
  (`.scratch/<feature-slug>/`). `docs/agents/triage-labels.md` (new this
  session): 3-value status vocabulary — `ready-for-agent`, `ready-for-human`,
  `done`. Update each ticket's `Status:` line to `done` as it's completed and
  its acceptance check passes.
- `docs/agents/domain.md` + `CONTEXT.md` + `docs/adr/`: single-context domain
  docs. `CONTEXT.md` already has "Pure-water chemistry", "Scavenger", and
  "Bulk species" defined — use that vocabulary, don't invent synonyms.
- Global `~/.claude/CLAUDE.md` ground rule still applies: never guess
  Geant4-DNA API behavior — verify against source or by running it. This
  session caught several wrong assumptions this way (see above); expect more.

## Suggested skills for the next session

- Call `Skill` with `superpowers:executing-plans` early — this is exactly a
  written, ticketed implementation plan meant to be executed in a fresh
  session with review checkpoints per ticket.
- Call `Skill` with `superpowers:verification-before-completion` before
  marking any ticket `done` — every ticket's acceptance check is an exact
  command + expected result; run it for real, don't infer a pass.
- If an acceptance check doesn't behave as the ticket predicts (plausible —
  ticket 07 in particular flags a real risk of MT floating-point-order
  flakiness), call `Skill` with `superpowers:systematic-debugging` rather
  than guessing at a fix.
