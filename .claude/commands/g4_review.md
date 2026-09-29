Review this Geant4-DNA (v11.4.1) project and write the findings to a Markdown file that
`/gps scout --from` can turn into sessions. This is read-only: do not edit code,
fix builds, or commit. Read-only commands (git, grep, reading CMake) are fine;
do not build unless I ask.

## Scope
Target: $ARGUMENTS (branch, PR number, path, or empty).
- Empty + uncommitted changes → review `git diff HEAD`, reading surrounding code as needed.
- Empty + clean tree → review the whole project, excluding build/, generated and vendored code.
- Linked libraries (e.g. G4Vox) are dependencies: review how this project calls them,
  not their internals, unless I name them as the target.
Don't stop to ask about scope; pick the most reasonable reading and state it in the report.

## Step 1 — Facts before judgment
1. Read CLAUDE.md, REVIEW.md, CONTRIBUTING and any docs/ conventions. Their priorities
   override the defaults below.
2. From CMake/config, record: C++ standard, Geant4 version, run manager type (MT/tasking/
   sequential), compiler/platform assumptions.
3. Record how Geant4-DNA is wired: physics constructor (G4EmDNAPhysics_optionN or custom),
   region-specific activation, materials in DNA regions, energy ranges, chemistry
   (G4EmDNAChemistry*, time-step model SBS/IRT, end time, reaction table, scavengers).
4. Map entry points: main(), ActionInitialization (Build/BuildForMaster), DetectorConstruction
   (Construct/ConstructSDandField), primary generator, run/event/tracking/stepping actions,
   time-step/chemistry actions, analysis/output.
When a finding depends on Geant4 API behaviour, check the installed headers/sources of the
detected version. If you can't verify, say so and lower the confidence.

## Step 2 — Review, in priority order
1. Silent physics errors: DNA models outside their validity (material, energy range), wrong
   constructor for the stated intent, missing units (eV, nm, ps), double-counted or mis-scored
   energy deposits, wrong step/track status logic, chemistry disabled or misconfigured.
2. MT correctness and reproducibility: shared mutable statics/globals/singletons without
   G4ThreadLocal, SDs or fields built outside ConstructSDandField, actions in the wrong
   Build/BuildForMaster, incorrect run merging (G4Accumulable, G4AnalysisManager),
   per-thread output file collisions, RNG seeding, chemistry thread safety.
3. Lifetime and memory: deleting kernel-owned objects, leaking user-owned ones, pointers
   dangling across runs, per-event allocations that grow, UB (uninitialized members,
   out-of-bounds, signed overflow).
4. API misuse and error handling: unchecked nullptr from G4 lookups (materials, volume
   stores, particles), G4Exception usage, messenger commands without validation,
   geometry/physics changes between runs without notifying the kernel.
5. Performance: I/O, string building or store lookups inside SteppingAction; per-step allocation.
6. Maintainability: only where the repo's conventions require it or it causes real confusion.

Rules:
- Every finding cites file:line and evidence. No evidence, no finding.
- If something looks deliberate (unusual DNA config, custom model), put it in Open questions.
- Fewer well-supported findings beat many speculative ones.
- For a large codebase, split areas across subagents (physics/geometry, actions/MT, chemistry,
  output), then merge and deduplicate.

## Step 3 — Write the report
Write `docs/reviews/YYYY-MM-DD-<short-slug>-review.md` with exactly this structure:

# <Project> review — <target>
**Target:** … · **Commit:** <short sha> · **Date:** …
**Environment:** C++NN · Geant4 x.y.z · MT/sequential · DNA physics: … · Chemistry: …

## Summary
3–5 sentences, ending with the IDs to fix first.

## Architecture tour
At most 20 lines: entry points and how Geant4-DNA is wired.

## Findings
### F-001 — <short title>
- **Severity:** Critical | High | Medium | Low
- **Area:** physics | MT | memory | API | performance | tests | maintainability
- **Location:** `src/File.cc:42` (`Class::Method`)
- **Confidence:** Confirmed | Likely | Needs check
- **Problem:** what is wrong and its effect (results, crash, reproducibility)
- **Evidence:** snippet of 10 lines max, or the code path
- **Fix direction:** concrete change, short snippet if useful
- **Verify with:** macro, test or command that proves the fix
- **Related:** F-00x

Sort by severity and number sequentially. Never renumber once written.
Severity: Critical = silently wrong results, or crash/UB in normal runs including MT.
High = wrong in plausible configs (MT, other DNA option, repeated beamOn), growing leak,
broken reproducibility. Medium = fragile, missing validation, perf that matters at scale.
Low = clarity, conventions.

## Testing gaps
Each gap: what to add, which findings it would catch, how to run it. Consider: fixed-energy
single-particle runs with a known reference, MT vs sequential with fixed seeds, two
consecutive /run/beamOn, each supported DNA physics option, chemistry on/off.

## Suggested sessions
Group findings into ordered work sessions, each shippable as one PR:
### S1 — <kebab-case-slug>
- **Findings:** F-001, F-004
- **Goal:** one sentence describing the outcome
- **Why together:** shared root cause or file
- **Depends on:** none | S2
Don't bundle Critical fixes with Low cleanup.

## Open questions
Q1 — … (F-00x). Only things I need to decide.

## Not reviewed
What was skipped and why.

Finish with a short chat message: the report path, the count per severity, and the line
`/gps scout --from <path>`.