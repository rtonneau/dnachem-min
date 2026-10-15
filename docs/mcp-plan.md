# Plan: a general MCP server for Geant4 / Geant4-DNA applications

Status: **proposal, plan only.** Nothing here is implemented. Date: 2026-10-15.
Reference consumer: `dnachem-min`. Geant4 reference: the `rtonneau/geant4`
tree (11.4.1 layout), read-only, shallow clone, not built.

Working name for the server: **`g4mcp`** (open question 1).

Things I could **not** verify while writing this (marked *[verify]* where they matter):

- the current `rmcp` API (no network lookup done);
- which Geant4 data sets each DNA example strictly needs at start-up;
- the exact semantics/units of the `sumG`/`sumG2` columns (read from `ScoreSpecies`
  and the `sim-output` skill before relying on them).

Everything else cites a file I read in this session.

---

## 0. Summary of recommendations

1. **Own repo, Rust, stdio MCP server, one binary.** Each Geant4 project commits
   one small file, `g4mcp.toml`; the server is registered once at user scope.
2. **Jobs outlive the server.** A long chemistry run (hours) must survive a Claude
   Code restart, so each run is owned by a small detached *supervisor* process and
   a directory of plain files, not by the MCP server's memory.
3. **Never trust the exit code alone.** Classify a run from exit code + log markers +
   expected outputs + `Manifest.json`, exactly the checklist already written in
   `.claude/geant4-instructions.md` section 4, but done by the server.
4. **Macros are the configuration surface.** Geant4 already has `/control/alias`
   and `{alias}` substitution; use that plus a small, recorded text *overlay*
   (pre-init lines, post-`/run/initialize` lines, line substitutions). Always save
   the final macro with the run.
5. **No code-generating scaffold tools in v1.** Claude Code already writes Geant4 C++
   (scorers, geometry, reactions) well. What it lacks is this project's conventions
   and the Geant4-DNA pitfalls (second `/chem/` directory, `Merge`, irreversible
   `SetReactionType`, command ordering). That is documentation: it lives in
   `docs/recipes/` plus a cross-project skill (section 5), not in server tools. The
   server contributes only two checks against the real app (`ui_commands`,
   `project_check`).
6. **dnachem-min conventions worth exporting**: `--dir` output redirection,
   dump-and-reset with a `Manifest.json`, portable std-only counters with
   `Merge`/`Clear`/`WriteAscii`/`WriteCsv`, named Chemistry registry, the
   `header/<cluster>/` layout. **Conventions not worth exporting**: hard-coded
   defaults (e.g. `PrimaryKiller` 500 keV / 1 MeV, end time set in
   `ActionInitialization::Build`) that silently change results.
7. **First milestone is "run and summarise dnachem-min's `example_irt.in` from
   Claude on Windows"**, because it forces the hard parts (env, DLLs, supervisor,
   markers, manifest) before anything else is built.

---

## 1. What the survey found

### 1.1 Run contract, inferred

Sources read: `dnachem-min/sim.cc`, `src/core/{ArgParser,MacroResolver,OutputDir}.cc`,
`CMakeLists.txt`, `.claude/geant4-instructions.md`, `.claude/.claude-project.json`,
`docs/output/README.md`, `.claude/skills/sim-output/SKILL.md`; Geant4
`examples/extended/medical/dna/{chem1,chem6,scavenger,UHDR}/*.cc|*.in`,
`source/run/src/G4RunManagerFactory.cc`, `cmake/Modules/G4DatasetDefinitions.cmake`,
`cmake/Templates/geant4-bat-skeleton.in`.

| Aspect | Upstream DNA examples | dnachem-min | What the generic contract must express |
|---|---|---|---|
| Entry point | one executable per example, named after the dir (`chem6`, `UHDR`); no args → interactive `G4UIExecutive`, with an arg → batch (`chem6.cc`, `UHDR.cc`) | `sim`; batch only (`useGUI=false` const in `sim.cc`) | executable path, plus which args select batch mode |
| Macro arg | `argv[1]` passed verbatim to `/control/execute` (`chem6.cc`) | `argv[1]`, resolved as given → `<exeDir>/<arg>` → `<exeDir>/macro/<arg>` (`MacroResolver.cc`) | macro resolution rule, search dirs |
| Other args | `UHDR`: `argv[2]` = seed (`UHDR.cc:50`); `chem1`: own `CommandLineParser` | `--threads N`, `--dir <path>` via `ArgParser` | declared, typed parameters, each mapped to a flag or to a macro command |
| Threads / run manager | `G4RunManagerFactory::CreateRunManager()` default type; `/run/numberOfThreads N` in macro (`chem6/beam.in` line 1); env `G4RUN_MANAGER_TYPE`, `G4FORCE_RUN_MANAGER_TYPE` read by the factory (`G4RunManagerFactory.cc:79-87`) | Serial by default, `--threads N` → MT; macro may still override | threads as a parameter with a delivery channel (flag / macro / env) |
| Seed | `UHDR`: argv; others default CLHEP | `kDefaultSeed=12345` in `sim.cc`, `/random/setSeeds a b` in macro | seed as a parameter (macro command or flag); *recorded*, because output is not bit-reproducible (ADR 0005) |
| Working dir | macros copied to the build dir at configure/build (`chem6/CMakeLists.txt`, `configure_file(... COPYONLY)`), outputs written to **cwd** (`out.open("Species.txt")` in `chem6.cc`, ROOT/CSV via `G4AnalysisManager`) | macros copied by a `POST_BUILD` step; outputs go to `--dir`, else `<exeDir>/results` | cwd rule, output-redirect rule, "macros stale until rebuild" flag |
| Output | `Species.txt`, ROOT files (22 of 32 example dirs call `SetDefaultFileType("root")`, 1 xml, 1 csv) | text + CSV ntuples + HDF5 (optional) + `Manifest.json` per dump | per-file *reader kind*; optional manifest |
| Data sets | env vars set by `geant4.bat` / `geant4.sh`; DNA models call `G4FindDataDir("G4LEDATA")` and raise a fatal `G4Exception` if missing (`G4DNABornExcitationModel2.cc:103-108`) | same | env discovery and validation (section 6) |
| Progress | `/run/printProgress N` prints `--> Event N starts.` (`G4RunManager.cc:481`); `### Run N starts.` (`:360`) | adds `[RunAccumulatorMessenger] dumped and reset`, `The simulation took` | regex-based progress and success markers, plus an injected `/run/printProgress` |
| Failure | `G4Exception` blocks (`-------- EEEE ------- G4Exception-START`, `*** G4Exception : <code>`), process exit | same; MSVC adds `0xC0000135` (DLL not found), `0xC0000005` | a classifier combining both |
| Cost | seconds (physics) to hours (chemistry to 1 s); MT multiplies memory (note in `.claude-project.json`) | same, plus a harness that kills jobs under memory pressure | timeout, stall watchdog, memory cap, admission control |

**Hazards found that shape the design**

- A killed run writes nothing in dnachem-min: data is only written by
  `/run/dumpDataAndReset*`, and the `EndOfRun_` safety net runs at normal exit
  (`sim.cc`). Cancel = lose everything since the last dump. The example macros
  already dump per sub-run (`sub_01/`, `sub_02/`); the server should recommend
  that and expose partial results per dump.
- `/control/shell` exists (`G4UIcontrolMessenger.cc:179`). A server that runs
  macros written by an LLM must be able to refuse it (config `macro.forbidden`).
- Some commands only work after `/run/initialize` and some only before; and
  dnachem-min overwrites the chemistry end time inside `/run/initialize`
  (`ActionInitialization::Build`), so `/scheduler/endTime` must come *after* it
  (`CLAUDE.md`, Testing). Any "set X for me" feature therefore needs an
  *anchor* in the macro, not just a prepended line.
- Upstream macros are not always ASCII: `chem6/beam.in` contains `°OH`. The
  server must treat macros as bytes and never re-encode them.
- A Debug test binary linked against a differently-built Geant4 install corrupted
  memory once (`CLAUDE.md`, `ReactionCounter` entry). The server should record
  build type with each run and warn on mixes.

### 1.2 Extension points, counted

Counted over the 32 top-level directories of `examples/extended/medical/dna`
(`grep` on class names, one hit per directory; `molcounters` has two sub-examples
but counts once). Line counts are `wc -l` of the `.cc` of a representative file.

| Extension point | Dirs using it (of 32) | Typical size | Notes |
|---|---|---|---|
| `G4VUserDetectorConstruction` | 32 | 100-140 LOC (`chem6` 136) | water box is the common case; real geometries (pdb4dna, wholeNuclearDNA, neuron) are not templatable |
| `G4VUserPrimaryGeneratorAction` (gun / GPS / SingleParticleSource) | 32 | 60-100 (`chem6` 66, `UHDR` 102) | |
| `G4VModularPhysicsList` | 32 | 55-120 | choice of `G4EmDNAPhysics[_optionN]` is a one-liner; 28 dirs include a `G4EmDNAPhysics*` header |
| `G4UserRunAction` | 30 | 60-120 | |
| any UI command / messenger | 31 | 60-130 each; explicit `G4UImessenger` subclasses in 25 dirs, 73 `*Messenger` classes counted | highest repeat count, many per project |
| `G4AnalysisManager` | 27 | scattered | ROOT 22, xml 1, csv 1 |
| `G4UserSteppingAction` | 21 | 20-375 (`dnaphysics` 375) | |
| `G4UserEventAction` | 18 | 50-90 | |
| `G4Run` subclass (`Merge`) | 16 | 110-140 (`chem6` 136) | MT merge logic; easy to get wrong |
| `G4UserStackingAction` | 13 | 65 | chemistry examples mostly |
| `G4DNAChemistryManager` use | 14 | | |
| scorer (`G4VPrimitiveScorer` in 7 dirs: UHDR chem4 chem5 chem6 dnadamage2 molcounters scavenger; `G4VSensitiveDetector` in 3) | 7 (+3) | 300-615 (`chem6` ScoreSpecies 305, `UHDR` Scorer 615) | G-value / species / dose; MT-merged |
| `G4UserTimeStepAction` | 8 | 90-300 (`scavenger` 92) | |
| `G4Scheduler` configuration | 12 | few lines | |
| custom chemistry list (`G4VUserChemistryList` / chemistry `G4VPhysicsConstructor`) | 3 (UHDR, dnadamage2, scavenger); 13 dirs use stock `G4EmDNAChemistry*` | 470 (`scavenger/EmDNAChemistry.cc`) + data | reaction tables, scavengers, pH |
| molecule counters (`G4MoleculeCounterManager`) | 8 | 100+ | `molcounters/{basic,sdcounters}` |
| custom CLI parser | 2-3 (`chem1` `CommandLineParser`) | 130 | dnachem-min `ArgParser` is the compact version |

---

## 2. Feature catalog: what recurs, and where the knowledge should live

Ranking = recurrence × boilerplate × cost of getting it wrong. "dnachem-min
status" says whether the project already has a good pattern to copy. The catalog
is the **content of the recipe skill** described in section 5 (checklists, files
touched, registration sites, pitfalls); it is not a list of server templates.

| # | Feature | Recurrence | Boilerplate | Priority | dnachem-min pattern worth copying |
|---|---|---|---|---|---|
| 1 | **App skeleton** (main, ActionInit, Detector, PrimaryGen, PhysicsList, Run/RunAction, CMake, macro, `g4mcp.toml`) | 32/32 | ~600 LOC | P0 | `sim.cc`, `CMakeLists.txt` (`GLOB_RECURSE ... CONFIGURE_DEPENDS`), mirrored `src/`+`header/` |
| 2 | **Macro command (messenger)** | 25-31/32, several per project | 60-130 LOC each, plus a known pitfall | P0 | `ChemistrySelectMessenger`, `MesoMessenger`, `DnaChemistryWorld` (`G4GenericMessenger`) |
| 3 | **Per-run counter / accumulator** (`Add/Merge/Clear/WriteAscii/WriteCsv`, fed from `RunAction`, merged in `Run::Merge`, dumped) | 16/32 have a `Run` subclass; accumulation across `beamOn` is not upstream | 150-250 LOC | P1 | `PhysicsInteractionCounter` (38 LOC .cc), `RunAccumulator`, `RunAccumulatorMessenger` |
| 4 | **Scorer** (species / G-value / dose / LET; `G4VPrimitiveScorer` + `G4MultiFunctionalDetector`) | 7 (+3) | 300-600 LOC | P1 | `ScoreSpecies`, `PrimaryKiller`, registration in `DetectorConstruction::ConstructSDandField` |
| 5 | **Chemistry setup** (molecules, dissociation, reaction table, bulk reactions, scavengers, time-step model) | 13 use, 3 customise | 180-470 LOC + data table | P1 | `ChemistryRegistry` + `catalog/*Reactions.cc`, `ChemistryTypes::BulkReactionList` as plain data |
| 6 | **Actions**: stepping / tracking / event / stacking | 12-21 each | 20-120 LOC each | P2 | `SteppingAction` (21 LOC), `TrackingAction` + `TrackLengthTable` |
| 7 | **Time-step action** (per-step chemistry hook, hand-over) | 8 | 90-300 | P2 | `TimeStepAction` (IRT_syn → mesoscopic) |
| 8 | **Physics constructor choice** (`G4EmDNAPhysics` option 1-8, `G4EmDNAChemistry` option 1-3, `G4EmParameters`) | 28 | one line + includes | P2 — a one-line **edit**, not a class | `PhysicsList.cc` edits the constructor directly, no string dispatch |
| 9 | **Primary generator variants** (gun, GPS, spectrum, pulse) | 32 exist, few change | 60-100 | P3 | `PrimaryGeneratorAction` (60 LOC) |
| 10 | **Geometry** (water box, chemistry world with scavengers/pH/boundary) | 32 | 100-150 | P3 | `DetectorConstruction` + `DnaChemistryWorld` (`/chem/env/*`) |
| 11 | **Molecule counter** (`G4VUserMoleculeCounter`) | 8 | 100+ | P3 | project uses its own counters instead |
| 12 | **Structured output** (`DataNode` + `JsonWriter` + `Manifest.json`, HDF5 writer) | 0 upstream | 100-300 | P1 for *consumption* by the server (section 4.3) | `RunManifest`, `JsonWriter`, `MesoSpatialFile` |

Rule of thumb: P0/P1 items get a full recipe (steps, files, registration, pitfalls);
P2 items a short one; P3 items are links to upstream examples. Code generation is
reconsidered only if Claude keeps making the same multi-file mistake with the recipes
in context (section 5.4).

### 2.1 Per-feature detail

For each: **recipe** (the parameters are the questions to settle before
writing code), **files touched**, **registration**, **upstream divergence**. Paths: `G4:` = `rtonneau/geant4`, `DM:` = `dnachem-min`.

#### 1. App skeleton

- **Recipe** `app.skeleton`: parameters `name`, `layout` (`upstream-flat` |
  `clustered`), `physics` (`G4EmDNAPhysics_option2` default), `chemistry`
  (`none` | `stock:G4EmDNAChemistry_option3` | `registry`), `threads_cli` (bool),
  `output_dir` (bool, adds `OutputDir`+`--dir`).
- **Files** created: `main`, `ActionInitialization`, `DetectorConstruction`,
  `PrimaryGeneratorAction`, `PhysicsList`, `Run`, `RunAction`, `CMakeLists.txt`,
  `macro/beam.in`, `g4mcp.toml`, a `README` stub.
- **Registration**: `SetUserInitialization` ×3 in `main`; `Build()`/`BuildForMaster()`
  in `ActionInitialization`.
- **Upstream**: `G4:examples/extended/medical/dna/chem6/` is the closest model
  (flat `include/`+`src/`, `file(GLOB sources src/*.cc)`, macros copied via
  `configure_file`).
- **DM divergence**: `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` (new files picked
  up automatically; upstream globs without `CONFIGURE_DEPENDS` and non-recursively),
  includes rooted at `header/` (ADR 0003), macros copied by `POST_BUILD`
  `copy_directory` (needed for multi-config generators, comment in `CMakeLists.txt`),
  output directory abstraction and macro resolver in `main`.

#### 2. Macro command (messenger)

- **Recipe** `ui.command`: parameters `directory` (e.g. `/myapp/env/`), `name`,
  `type` (`bool|int|double|doubleUnit|string`), `states` (`PreInit|Idle|...`),
  `target` (a setting struct or a class), `style` (`plain` | `generic`),
  `directory_exists` (bool).
- **Files**: `header/<cluster>/XMessenger.hh`, `src/<cluster>/XMessenger.cc`,
  optionally a settings struct; test in `test/` if validation is pure.
- **Registration**: construct the messenger in the owner's constructor, or in
  `main` (as `sim.cc` does for `DnaLoggerMessenger`, `OutputDirMessenger`,
  `RunAccumulatorMessenger`). *Order matters* if the directory is shared: the
  owner of `/chem/env/` must be constructed before `ScavengerMessenger`
  (`DnaChemistryWorld.cc:20-33`).
- **Decision rule in the recipe** (from `docs/recipes/create-a-macro-command.md`):
  if the directory already exists (another component created it, e.g.
  `/chem/` by `G4DNAChemistryManager`), generate a **plain `G4UImessenger`**;
  a `G4GenericMessenger` would try to create a second `/chem/`. If the directory
  is new, `G4GenericMessenger` is acceptable, but use `DeclareMethod` for anything
  that must validate, not `DeclareProperty`.
- **Upstream**: both styles occur (`G4:.../UHDR/src/PulseActionMessenger.cc` plain,
  `chem6/src/PhysicsListMessenger.cc` plain; `G4GenericMessenger` in one dir).
  Upstream puts commands under the example's own root (`/UHDR/...`); DM mixes
  `/chem/...` (shared with Geant4), `/run/...` (output) and `/primaryKiller/...`.
  A recipe for a new project should default to a **project-owned root** and warn when the
  requested directory belongs to Geant4 (`/run/`, `/chem/`, `/process/`).
- **Validation outside the recipe**: after writing the command the server can run the app
  with `/control/manual <dir>` to prove the command registered (tool `ui_commands`).

#### 3. Per-run counter / accumulator

- **Recipe** `scoring.counter`: parameters `name`, `key` type (`string` | `int` |
  `(string,time-bin)`), `value` type, `outputs` (`txt`, `csv`), `feed` (`step` |
  `track` | `event` | `run`).
- **Files**: `header/scoring/X.hh`, `src/scoring/X.cc`, `test/XTest.cc`, patches to
  `Run` (member + `Merge`), the feed site (`SteppingAction`, etc.), `RunAccumulator`
  (member, `Add`), `RunAccumulatorMessenger` (dump), and the manifest's `files`.
  The recipe `docs/recipes/add-a-per-run-counter.md` already lists these steps; the
  recipe already exists as that checklist.
- **Registration**: via `Run::Merge` (master), `RunAction::EndOfRunAction`,
  dump hook. CMake test target appended to `CMakeLists.txt` (`add_executable(XTest
  test/XTest.cc src/scoring/X.cc)` + `add_test`), which is a hand-edited block in DM.
- **Upstream**: no equivalent; upstream accumulates inside the scorer and writes at
  `EndOfRunAction` through `G4AnalysisManager`. The DM pattern (std-only class,
  stream-only I/O, `Merge(other)` adds into `this`) is strictly better for testing
  and reuse and is the one to export.

#### 4. Scorer

- **Recipe** `scoring.primitive-scorer`: parameters `name`, `quantity`
  (`edep|species|gvalue|let|track-length|custom`), `volume` (logical volume name),
  `time_bins` (log / list), `ntuple` (bool).
- **Files**: `header/scoring/X.hh`, `src/scoring/X.cc`; patch `DetectorConstruction::
  ConstructSDandField`; patch `Run` for merge if not purely SD-hosted.
- **Registration** (verified in `docs/recipes/create-a-scorer.md`, `DM:src/geometry/
  DetectorConstruction.cc:124-146`): `G4MultiFunctionalDetector::RegisterPrimitive`
  → `G4SDManager::AddNewDetector` → `SetSensitiveDetector("World", mfDetector)`.
- **Upstream**: `G4:.../chem6/src/ScoreSpecies.cc` (305 LOC) is the base;
  `UHDR/src/Scorer.cc` (615 LOC) adds dose cut-off and `/scorer/Gvalues/...`.
  DM `ScoreSpecies` (436 LOC) is derived from chem6's, so a new scorer should
  start from DM's ntuple layout (`speciesID, number, nEvent, speciesName, time,
  sumG, sumG2`, `ScoreSpecies.cc:351-358`) because the result reader depends on it.
- **DM divergence**: persistent (never reset by the run manager) with explicit
  dump-and-reset; `G4AnalysisManager::Clear()` after each dump, to avoid the
  analysis-manager file-registry limitation documented in `sim-output`.

#### 5. Chemistry setup

- **Recipe** `chemistry.named-chemistry`: parameters `name`, `base`
  (`PureWater|BoscoloChem|empty`), `extra_molecules` (list), reaction rows
  (CSV/inline: `A + B -> C + D, k, type`), `bulk_reactions`.
- **Files**: `header/chemistry/catalog/XReactions.hh`, `src/chemistry/catalog/
  XReactions.cc` (portable pair, `ChemistryTypes.hh` only), one line in
  `src/chemistry/BuiltInChemistries.cc`; optional `XTable.cc` plain-data table and
  `test/XTableTest.cc` (pattern: `Tonneau2025Table`).
- **Registration**: `ChemistryRegistry::Register(name, builders)`; selection with
  `/chem/select <name>` before `/run/initialize`.
- **Rules to bake in** (from `docs/recipes/add-a-new-chemistry.md`): look species
  up by name and fail fatally on unknown; call `SetReactionType(1)` only if
  `ChemistryTypes::PartialReactionsEnabled()` (cannot be undone, SBS must stay
  type 0); bulk list is plain data.
- **Upstream**: three styles exist and the recipe should name them: (a) macro
  `/chem/reaction/add` after `/chem/reaction/UI` (chem6 `beam.in`), (b) reaction
  file `Rtable_*.txt` parsed by `ParserChemReaction` (`G4:.../scavenger/`), (c) C++
  builders (`G4:.../UHDR/src/Chem*Builder.cc`, `ChemistryWorld.cc`). DM follows
  (c) and adds the registry; its `CLAUDE.md` forbids (a) because
  `/chem/reaction/UI` wipes the class-built table.
- **Also an input to this recipe**: `docs/literature/` and the Table-2 style data
  sheet pattern (`Tonneau2025Table.cc`): the table as data, the builder as logic.

#### 6. Actions (stepping / tracking / event / stacking)

- **Recipe** `action.{stepping,tracking,event,stacking}`: minimal class plus
  optional `counter` hook (calls feature 3).
- **Files**: `header/actions/X.hh`, `src/actions/X.cc`; patch `ActionInitialization::
  Build` with `SetUserAction(new X)`.
- **Registration**: `ActionInitialization::Build()` (workers) and, for run-level
  ones only, `BuildForMaster()`.
- **Upstream** `G4:.../dnaphysics/src/SteppingAction.cc` (375 LOC of histogramming) is
  atypical; DM's 21-LOC stepping action that records a process name into a counter
  is the shape to copy.

#### 7. Time-step action

- **Recipe** `chemistry.timestep-action`; patch
  `G4Scheduler::Instance()->SetUserAction(new X(...))` in `Build()` under
  `if (G4DNAChemistryManager::IsActivated())` (exact form in
  `DM:src/actions/ActionInitialization.cc`).
- **Upstream**: `G4:.../scavenger/src/TimeStepAction.cc` (92 LOC), `chem2`, `chem3`.
- **DM**: its `TimeStepAction` is large (hand-over to the mesoscopic stage). Recipe
  only the upstream-sized one; the hand-over is project-specific.

#### 8. Physics choice (one-line edit)

- Edit **one** declaration. Valid values enumerated from
  `G4:source/physics_lists/constructors/electromagnetic/include/G4EmDNAPhysics*.hh`
  (base, `_option1`..`_option8`, `_stationary*`) and `G4EmDNAChemistry*.hh`
  (base, `_option1`..`_option3`).
- **Divergence**: upstream `chem6` supports `/chem/phys/registerPhysics <name>`
  through a string→factory map (`chem6/src/PhysicsList.cc`, `PhysicsListMessenger.cc`).
  DM deliberately has no dispatch: change the constructor line (`PhysicsList.cc`,
  with the old default kept in a comment). Recommendation: write DM's way by
  default; offer `registerPhysics` as an opt-in because it lets a *macro* choose,
  which is what an MCP "configure run" wants.
- **Parameter-in-macro** alternatives that need no code: `/process/dna/e-SolvationSubType`
  (used in `chem6/beam.in`, `UHDR/beam.in`), `/process/chem/TimeStepModel`
  (`SBS|IRT|IRT_syn`; DM accepts SBS and IRT_syn only, classic IRT is fatal).

#### 9-11. Generator / geometry / molecule counters

Documentation links first (`create-a-*` recipes, upstream example dirs). Recipe
only the water box + `DnaChemistryWorld`-style `/…/env/{halfBox,pH,scavenger}`
commands, because that block repeats across the chemistry examples (`UHDR`
`/UHDR/env/*`, DM `/chem/env/*`).

#### 12. Structured output

**Consumed** by the server, not generated: `Manifest.json` (`schemaVersion` 1) is
the one app-side convention the server should treat as first-class (section 4.3).
`DataNode`/`JsonWriter` are std-only and portable, so a recipe for copying them into
another project is cheap to write.

### 2.2 Where DM diverges from upstream (summary)

| Topic | Upstream | dnachem-min | Take |
|---|---|---|---|
| Layout | flat `include/`+`src/` | mirrored `header/<cluster>/`+`src/<cluster>/`, includes rooted at `header/` | keep as `layout=clustered`; support both |
| CMake sources | `GLOB src/*.cc` | `GLOB_RECURSE ... CONFIGURE_DEPENDS` | adopt |
| Physics choice | string dispatch in chem6 | direct constructor | direct by default |
| Chemistry content | macro rows, `Rtable_*.txt`, or C++ builders | C++ catalog + registry + `/chem/select` | adopt registry for new projects with >1 chemistry |
| Command roots | per-example (`/UHDR/...`) | shares `/chem/` with Geant4 | warn on shared roots; plain messenger when dir exists |
| Output | cwd, `G4AnalysisManager` ROOT, global `std::ofstream out` | output-dir abstraction, `--dir`, dump-and-reset, manifest, CSV/HDF5 | adopt (it is what makes runs machine-readable) |
| Reproducibility | default seed | fixed 12345 + `/random/setSeeds`; recorded, not bit-reproducible | record, never promise |
| Hidden defaults | few | `PrimaryKiller` eLossMin 500 keV / eLossMax 1 MeV always applied (`DetectorConstruction.cc:136-146`); end time 1 s/1 µs set in `Build()` | **don't export**; recipes say to make defaults explicit and log them |
| Macro lookup | `argv[1]` verbatim | 3-step resolver | adopt resolver; server stages macros anyway |
| Run manager | factory default type | Serial default, `--threads` | adopt (Serial first is how the debugging notes say to work) |

---

## 3. Architecture

```
Claude (Code / Desktop)
   │ stdio, JSON-RPC (MCP)
   ▼
g4mcp serve  ── project registry ── g4mcp.toml (per project)
   │  tools: run.* , results.* , env.* , check.*
   │  resources: run logs, output files
   │
   ├── spawns ► g4mcp supervise <job-dir>   (detached, one per run)
   │               ├─ Windows Job Object (kill tree, memory cap)
   │               ├─ child: sim.exe <args>   stdout+stderr → job-dir/run.log
   │               ├─ watchdogs: wall time, no-output stall, memory
   │               └─ writes job-dir/status.json, exit.json
   └── reads job dirs (plain files) to answer status/log/result tools
```

**Why a detached supervisor and a file-based job store.** Claude Code starts MCP
servers per session and stops them with it. A 1-s-end-time chemistry run is long.
If the server owned the child, closing the session would kill the run (or orphan
it with no one reading it). With a supervisor + directory:

- the server is stateless and restartable; `run_list` just scans `jobs/`;
- a crash of the server never loses a run;
- no database is needed in v1; the job directory is the per-run record (queueing,
  if added, is section 9).

Job directory (default `%LOCALAPPDATA%\g4mcp\jobs\<id>\`, `~/.local/state/g4mcp/jobs/<id>/`
on Linux; *not* inside the repo):

```
job.json        # request: project, build, macro source, params, overlay, limits
resolved.json   # argv, cwd, env diff (names + hashes, not values), exe path+mtime+build type
macro.final.in  # exactly what Geant4 executed (and macro.overlay.diff)
run.log         # merged stdout/stderr
status.json     # state, started, last_output_at, progress {events_done, run, ...}, pid
exit.json       # exit code/reason, classification, markers hit, warnings list
outdir/         # passed to the app (--dir) when the app supports it, else cwd = this dir
```

State machine: `prepared → running → {succeeded | failed | timed_out | stalled |
cancelled | oom_killed | lost}` (`lost` = supervisor disappeared; the server
detects it by a stale `status.json` heartbeat and a missing pid).

**Transport:** stdio only in v1 (local Windows machine, per-user). Streamable HTTP
is a later option for remote runs; the tool surface does not change.

**Long calls:** never block a tool call on a simulation. `run_start` returns in
< 1 s. `run_wait` blocks at most N seconds (default 30, max 120), emitting MCP
progress notifications if the client sent a progress token, and returns the
current status either way (Claude Code applies its own tool timeout).

**Output size:** every tool returns bounded text (default ≤ 8 KB, hard cap
configurable), with cursors for logs and tables. Large artefacts are exposed as
MCP *resources* (`g4mcp://jobs/<id>/run.log`, `.../outputs/<relpath>`), not inlined.

---

## 4. Job 1: tools to run simulations

All tools take an optional `project` (name from the registry; defaults to the one
whose `g4mcp.toml` is found by walking up from the client's working directory).
All return JSON text plus a one-line human summary. Errors are structured:
`{"error": {"code": "...", "message": "...", "hint": "..."}}`.

### 4.1 Discovery and environment

| Tool | Inputs | Output |
|---|---|---|
| `project_list` | `rescan?: bool` | `[{name, root, config_path, exe_found, builds:[{dir,type,generator,exe,mtime}]}]` |
| `project_info` | `project?` | resolved config (paths expanded, secrets masked), selected build, Geant4 install, warnings (e.g. macros older than sources when `macro.copied_at_build`) |
| `builds_find` | `project?` | candidate build dirs from `CMakeCache.txt` (`CMAKE_BUILD_TYPE`, `CMAKE_GENERATOR`, `Geant4_DIR`) and the executable path per build; marks which one is `role=run` / `role=test` |
| `env_check` | `project?`, `build?` | Geant4 install and version, data-set table (name, env var, path, exists, size-ish), DLL pre-flight (missing imports and where they were found), build-type mix warnings, pass/fail with hints (section 6) |
| `ui_commands` | `project?`, `prefix?` (default `/`), `refresh?` | command tree (path, guidance, parameters, types, defaults, allowed states), cached by exe mtime; produced by running the app once with a generated macro of `/control/manual <dir>` (guidance text is printed to stdout) |

### 4.2 Configure and launch

| Tool | Inputs | Output |
|---|---|---|
| `macro_list` | `project?`, `query?` | macro files with first comment line, size, mtime, whether stale vs source |
| `macro_read` | `project?`, `name`, `range?` | text (bytes preserved, shown as UTF-8 lossy with a flag) |
| `macro_validate` | `project?`, `name` or `text`, `overlay?` | per line: command exists, parameter count/type, state-order problems (e.g. `/scheduler/endTime` before `/run/initialize` for apps that set it in `Build()` - rule from config), forbidden commands (`/control/shell`), unknown aliases, `/run/beamOn` total, dump-prefix reuse (apps where reuse is fatal) |
| `run_prepare` | `project?`, `build?`, `macro` (name or inline `text`), `params?` (e.g. `{threads, seed, outdir_label}`), `overlay?`, `limits?` | a *dry run*: `{argv, cwd, env_diff, final_macro, overlay_diff, estimated_memory_mb, warnings, would_fail?}`; creates nothing |
| `run_start` | same as `run_prepare` + `label?`, `tags?`, `wait_for_slot?: bool` | `{job_id, state, job_dir, resolved}`; refuses if `env_check` fails hard or `forbidden` commands are present |
| `run_status` | `job_id` or `last` | `{state, elapsed_s, progress:{run, events_done, events_total?, eta_s?}, last_output_age_s, markers_seen, rss_mb, warnings_count}` |
| `run_wait` | `job_id`, `timeout_s?` (≤ 120), `until?: "any_progress"\|"done"` | status as above; emits progress notifications |
| `run_logs` | `job_id`, `cursor?`, `tail?`, `grep?`, `level?: "all"\|"geant4_exceptions"\|"warnings"\|"errors"`, `max_bytes?` | lines + next cursor; `geant4_exceptions` parses `G4Exception-START` blocks into `{severity, code, origin, text}` |
| `run_cancel` | `job_id`, `grace_s?` | terminates the tree (Windows: Job Object terminate; Linux: process group), reports which dump files, if any, exist; Geant4 has no graceful-abort path, so partial data is only what a dump already wrote |
| `run_list` | `project?`, `state?`, `tag?`, `limit?` | recent jobs, one line each |
| `run_clean` | `job_id` or `older_than`, `keep_manifest?`, `dry_run?: true` | frees job dirs; dry-run by default |

**How `params` reach the app (the generic part).** Each declared parameter has a
`channel` in `g4mcp.toml`:

- `flag`: appended to argv (`--threads 4`, `--dir <outdir>`);
- `macro-alias`: emitted as `/control/alias NAME VALUE` at the top of the final macro,
  for macros that use `{NAME}` (Geant4 native; works with unmodified Geant4 apps);
- `macro-command`: emitted as a command at a named *anchor*;
- `env`: set in the child environment (`G4FORCENUMBEROFTHREADS`, `G4RUN_MANAGER_TYPE` exist in Geant4).

**Macro overlay** (for existing, unparameterised macros such as dnachem-min's
`example_*.in`) is a small list of edits recorded as a diff:

```text
pre        = lines inserted before the first line
after_init = lines inserted after the first /run/initialize
replace    = [{match: "^/run/beamOn\s+\d+", with: "/run/beamOn 4"}]
append     = lines inserted at the end (typically /run/dumpDataAndReset)
```

Text-level and therefore fragile; this is deliberate. It is visible, diffable and
saved as `macro.overlay.diff`. The server rejects an overlay whose `after_init`
anchor is absent rather than guessing. For new apps the recipes should write
macros that use aliases, so overlays become unnecessary.

**Progress.** Inject `/run/printProgress N` (default `max(1, total/50)`) at `pre`
unless disabled. Progress regexes live in config; defaults are the Geant4 lines
`--> Event (\d+) starts` and `### Run (\d+) starts` plus app-specific ones. In MT
mode worker lines are interleaved; take the max event id per run. *[verify]* that
`--> Event N starts` appears on workers in the build used (printing is under
`printModulo`; `G4WorkerRunManager.cc:420`).

**Failure classification** (`exit.json.classification`): the first match wins, each
with a hint.

| Class | Evidence | Hint |
|---|---|---|
| `dll_not_found` | exit `0xC0000135` | run `env_check`; list missing DLLs; known dirs to add to PATH |
| `crash` | `0xC0000005`, `0xC0000409`, signal 11/6 | run Serial, Debug build, reduce events; check build-type mix |
| `geant4_fatal` | `EEEE`/`G4Exception` with `Fatal`/`core dump` | parsed exception code + origin |
| `app_error` | non-zero exit, no exception block | last 20 log lines |
| `missing_success_marker` | exit 0 but a configured success marker is absent | likely early return (e.g. macro not found) |
| `missing_outputs` | exit 0, markers fine, an expected output is absent/empty | the app may not have dumped (dnachem-min: no `/run/dumpDataAndReset`) |
| `timed_out` / `stalled` | wall limit / no output for `stall_s` | partial results listing |
| `oom_killed` | job memory limit hit, or OS kill with high RSS | reduce threads (memory scales per thread) |
| `warnings_only` | success but `WWWW` blocks not in `benign` | warnings listed once |

### 4.3 Collect and summarise

| Tool | Inputs | Output |
|---|---|---|
| `results_list` | `job_id` (or `dir`) | files with size, kind, mtime; if the app wrote a manifest/index, its dumps (dnachem-min: `Manifest.json` `dumps[]`) |
| `results_manifest` | `job_id`, `dump?` | the app manifest, trimmed to the keys the config marks as summary (beam, seed, energy deposit, Chemistry, threads, wall time) |
| `results_read` | `job_id`, `file`, `columns?`, `rows?`, `where?`, `format?: "table"\|"csv"` | a bounded slice of a CSV/ntuple/text table with column types |
| `gvalues` | `job_id`, `dump?`, `species?`, `times?` (e.g. `["1 ps","1 ns","100 ns","1 us"]`), `stage?` | G(species, t) with uncertainty: from the config-declared G-value file; for dnachem-min mean = `sumG/nEvent` and an SEM from `sumG2` *[verify formula]* |
| `reactions_top` | `job_id`, `time_window?`, `n?` | top firing reactions (dnachem-min `Reactions_nt_reactions.csv` + `ReactionsMetadata.csv`); generic when the config declares the file |
| `run_summary` | `job_id` | one page: classification, timing, key params, beam/energy/seed, counts, warnings, a G-value table at standard times, files |
| `runs_compare` | `job_a`, `job_b`, `metric?` | differences in params/overlay and in G-values, with a note when seeds differ (output is not bit-reproducible) |

**Reader kinds** (declared per output in `g4mcp.toml`): `text`, `csv`,
`geant4-csv-ntuple` (`G4CsvAnalysisManager` files: comment header lines then data;
*[verify]* exact header grammar), `json`, `hdf5` (v2: list groups, shapes, attributes
only; the crate needs the HDF5 DLLs the app already needs), `root` (**not in v1**:
upstream's default output is ROOT in 22 of 32 example dirs, so this is a real gap;
options in section 8).

**Why a manifest matters.** For dnachem-min, `Manifest.json` already carries
everything a summary needs (beam, seed, energy deposit, model, threads, wall time,
files). For apps without one, the server falls back to "files + reader kinds +
the log". The plan therefore documents the manifest as an *optional app-side
convention* (`output.manifest` in config) and documents it as a recipe (feature 12),
but does not require it.

---

## 5. Job 2: adding features to existing code (knowledge, not generators)

### 5.1 Decision

Claude Code already knows the generic Geant4 API and can write a scorer, change a
box, or add reactions from a prompt. A server that generates the same code adds
machinery (templates, anchors, patching) for little gain, and has to track every
project's layout. What Claude does *not* know without help is project- and
Geant4-DNA-specific: where things are registered, the order that matters, and the
traps. So this half is delivered as **documentation Claude reads**, and the server
adds only checks that need the real app.

### 5.2 Where the knowledge lives

| Layer | Content | Form |
|---|---|---|
| Per project (`dnachem-min/docs/recipes/`, already exists) | how *this* project does it: file paths, registration sites, test wiring, ADR links | markdown recipes, pointed to from `CLAUDE.md` ("before adding a scorer/command/chemistry, read the matching recipe") |
| Across projects (new skill, e.g. `geant4-dna-recipes`) | generic Geant4 / Geant4-DNA recipes and pitfalls, the content of section 2.1; upstream references (`examples/extended/medical/dna/*`) | Claude skill (progressive loading), kept in the `g4mcp` repo under `skills/` or in your user skills |
| Server | nothing generative | tools below |

Recipes to include first, in order of section 2: new app skeleton, macro command,
per-run counter, scorer, chemistry (named chemistry / reaction table / scavengers),
actions, time-step action, physics choice. Geometry: a short note only (not
templatable). Each recipe has the same skeleton dnachem-min's recipes use: what it
is, generic pattern, project reference implementation, Serial vs MT, see also.
Each carries the Geant4 version it was checked against.

### 5.3 The two server-side checks

| Tool | Inputs | Output |
|---|---|---|
| `ui_commands` | see 4.1 | proves a newly written command is registered (name, parameters, allowed states) by running the app once with `/control/manual` |
| `project_check` | `project?`, `rules?` | structural lint of the source tree against rules declared in `g4mcp.toml` `[check]`; reads files, does **not** compile. Examples: every class with `Merge` is called from `Run::Merge`; every portable class has a `test/XTest.cc` and an `add_test`; every messenger deletes its commands in the destructor; a `G4GenericMessenger` does not target a directory another component creates (`/chem/`, `/run/`, `/process/`); `SetReactionType(1)` is guarded by the partial-reactions switch; registration sites mention each new class. Output: list of `{rule, file, line, message}` |

`project_check` is optional and comes late (M6). Rules are plain regex/glob checks
declared per project, so it stays small; it can start with three rules taken from
the pitfalls above.

### 5.4 When to add generation after all

Only if, with recipes in context, Claude repeatedly gets a multi-file edit wrong
(for example the CMake test target, or the registration order). The first candidates
would be a skeleton generator and a messenger generator, as anchored insertions with
a plan/apply step. Not designed here.

---

## 6. How the server finds builds and the Geant4 environment

### 6.1 Finding builds (Windows and Linux)

Inputs, in priority order: explicit `[build]` in `g4mcp.toml`; else scan
`<project root>/build*/` for `CMakeCache.txt`.

From `CMakeCache.txt` read `CMAKE_BUILD_TYPE`, `CMAKE_GENERATOR`, `CMAKE_HOME_DIRECTORY`,
`Geant4_DIR`, `CMAKE_TOOLCHAIN_FILE` (as `.claude/geant4-instructions.md` already
instructs). Executable location:

1. **CMake File API** (`<build>/.cmake/api/v1/reply/`), if present: codemodel lists
   executable targets and artefact paths per configuration. Exact and works for
   Ninja and Visual Studio generators. A query file must exist *before* configure;
   `project_info` can offer to place `<build>/.cmake/api/v1/query/codemodel-v2` so the
   next configure creates it.
2. Else glob: single-config (Ninja, as dnachem-min uses) `<build>/<exe>[.exe]`;
   multi-config (Visual Studio) `<build>/<Config>/<exe>.exe`.

Roles: dnachem-min has two build dirs on purpose (`build/` RelWithDebInfo for runs,
`build-ninja/` Debug for ctest). `role=run` picks the first; the server refuses to
run simulations from the `role=test` Debug build unless `allow_debug_run` is set
(it works, but is slow and the asserts-vs-NDEBUG distinction matters for tests, not runs).
Staleness: warn if any source/macro is newer than the exe (`mtime`), particularly
`macro.copied_at_build` (macros reach the build dir only when the exe target builds).

### 6.2 Finding the Geant4 environment

Ordered attempts, first success wins, result cached keyed by (install path, script mtime):

1. `[geant4].env` explicit variables in the config (always applied last, so they win).
2. **Geant4's own env script.** Derive the install prefix from `Geant4_DIR`
   (`<prefix>/lib/cmake/Geant4` → `<prefix>`). Run `<prefix>/bin/geant4.bat`
   (Windows; template `cmake/Templates/geant4-bat-skeleton.in`) or `geant4.sh` (Linux)
   in a child shell followed by `set`/`env`, and diff against the base environment.
   This is the most faithful source for `PATH` (Geant4/CLHEP/Xerces DLLs) and for
   every `G4*DATA` variable the install knows about.
3. Variables already present in the server's process environment (`G4LEDATA`, ...).
4. Default install layout probing: `<prefix>/share/Geant4/data/<dataset-version>` for
   each dataset in `G4DatasetDefinitions.cmake` (`G4NDL`→`G4NEUTRONHPDATA`,
   `G4EMLOW`→`G4LEDATA`, `PhotonEvaporation`→`G4LEVELGAMMADATA`,
   `RadioactiveDecay`→`G4RADIOACTIVEDATA`, `G4PARTICLEXS`→`G4PARTICLEXSDATA`,
   `G4PII`→`G4PIIDATA`, `G4ENSDFSTATE`→`G4ENSDFSTATEDATA`, `G4ABLA`→`G4ABLADATA`,
   `G4INCL`→`G4INCLDATA`, `G4SAIDDATA`→`G4SAIDXSDATA`, `G4REALSURFACE`→`G4REALSURFACEDATA`,
   `G4CHANNELING`, ...).

**Geant4-DNA data.** The DNA cross sections live in `G4EMLOW` (`G4LEDATA`); the DNA
models resolve them through `G4FindDataDir("G4LEDATA")` and make a missing variable
fatal (`G4DNABornExcitationModel2.cc:103-108`; same pattern in the other
`models/src/G4DNA*`). `env_check` therefore treats `G4LEDATA` as **required** for any
app whose `[geant4].uses_dna = true`. `G4ENSDFSTATEDATA` and `G4PARTICLEXSDATA` are
checked and reported as "needed by most Geant4 physics" but whether each DNA-only
app strictly needs them is *[verify]*; the server warns, it does not block, except
where the Geant4 error itself said so.

### 6.3 DLL and shared-library resolution (the Windows problem)

dnachem-min needs: Geant4 DLLs (`<prefix>/bin`), CLHEP/Xerces (if shared), HDF5
(`hdf5::hdf5-shared`, vcpkg `installed/<triplet>/bin`), and, if the Qt UI driver was
linked, Qt (the machine-specific path in `.claude/geant4-instructions.md` shows this
bites: exit `0xC0000135`).

`env_check` performs a **pre-flight**, without running the app: parse the PE import
tables of the exe (`goblin`/`pelite` crate), resolve each import using the exact
search order the child will have (exe dir, then the PATH built in 6.2 + config
`run.env.path_prepend`), recurse one or two levels into non-system DLLs, and report
unresolved names with candidate directories found by searching the known roots
(`G4_ROOT`, vcpkg root from `CMAKE_TOOLCHAIN_FILE`, `CMAKE_PREFIX_PATH`). The fix is
proposed as a `path_prepend` entry for `g4mcp.toml`. Linux: `ldd`, or parse ELF
`DT_NEEDED` with the same crate.

The VC runtime (`vcruntime140*.dll`) normally comes from the system redistributable
and does **not** require `vcvars64.bat` at run time; `vcvars64.bat` is a *build-time*
need only.

### 6.4 Optional: build environment (not v1 scope)

If a `build_run` tool is added later: locate `vswhere.exe` under
`%ProgramFiles(x86)%\Microsoft Visual Studio\Installer`, get `installationPath` with
`-requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64`, run
`cmd /c "call \"<path>\VC\Auxiliary\Build\vcvars64.bat\" && set"` once, parse and cache
the environment, then run `cmake --build`. Exactly the procedure in
`.claude/geant4-instructions.md` section 1, including the `LNK1201` PDB workaround.
Kept out of v1 because the stated scope is "launch any built app".

### 6.5 Linux

Cheap and symmetrical: `geant4.sh` instead of `geant4.bat`, `ldd`/ELF instead of PE,
`/proc/self/exe` replaces `GetModuleFileNameW` in the app (DM already does this),
process-group kill instead of Job Objects, cgroups/`setrlimit` for memory. WSL is not
a goal; WSL-built apps are run through `wsl.exe` only if asked.

### 6.6 Conventions for paths

Everything uses `std::path` plus `dunce`-style simplification (no `\\?\` leaking to
Geant4), UTF-8 JSON, argv as `Vec<OsString>` (never a shell string), spaces in paths
supported, config values accept `${VAR}` expansion (as `.claude-project.json` does
with `${DEV_DIR}`, `${G4_ROOT}`).

---

## 7. Per-project config: `g4mcp.toml`

One file at the project root, committed. TOML (comments, no trailing-comma pain,
better for hand editing than the existing JSON). Keys deliberately reuse the names
already in `.claude/.claude-project.json` so migration is mechanical, and the server
can import that JSON as a fallback (`g4mcp import-claude-project`).

### 7.1 Schema (informal)

```toml
schema = 1

[project]
name        = "<id>"
kind        = "geant4-app"            # | "geant4-dna-app"
description = "..."

[geant4]
version      = "11.4.1"
install      = "${G4_ROOT}"           # prefix (contains lib/cmake/Geant4, bin/)
source       = "${DEV_DIR}/..."       # optional, read-only; used for recipe links and docs
env_script   = "auto"                 # "auto" | path | "none"
uses_dna     = true                   # makes G4LEDATA required
env          = { }                    # explicit overrides, win over everything
required_data = ["G4LEDATA"]          # extra data env vars to require

[build]
system      = "cmake"
executable  = "sim"
# one entry per build dir:
[[build.dirs]]
name = "run";  dir = "build";       type = "RelWithDebInfo"; role = "run"
[[build.dirs]]
name = "test"; dir = "build-ninja"; type = "Debug";          role = "test"

[run]
cwd            = "build-dir"          # "build-dir" | "output-dir" | "<path>"
smoke          = { macro = "example_irt.in", note = "..." }

# How the macro is passed
[run.macro]
dirs             = ["<exedir>", "<exedir>/macro"]   # where the app looks
source_dir       = "macro"                           # tracked macros
arg              = "filename-only"                   # | "path" | "flag:-m"
copied_at_build  = true
encoding         = "bytes"
forbidden        = ["/control/shell"]
order_rules      = [ { command = "/scheduler/endTime", after = "/run/initialize",
                       why = "Build() overwrites the end time at /run/initialize" } ]

# Typed parameters and where they go
[run.params.threads]
type = "int"; min = 1; max = "cores"; default = 0   # 0/absent = flag omitted (Serial)
channel = "flag"; flag = "--threads"
memory_per_unit_mb = 0                              # filled by user when known
[run.params.outdir]
type = "path"; channel = "flag"; flag = "--dir"; auto = "job-outdir"
[run.params.seed]
type = "int2"; channel = "macro-command"; command = "/random/setSeeds {a} {b}"; anchor = "pre"

[run.env]
path_prepend = []                     # filled by env_check suggestions
set          = { }

[run.limits]
wall_s   = 14400
stall_s  = 900
memory_mb = 0                         # 0 = no cap
max_concurrent = 1

[run.progress]
inject_print_progress = true
event   = '--> Event (\d+) starts'
run     = '### Run (\d+) starts'
done    = '\[RunAccumulatorMessenger\] dumped and reset'

[run.markers]
success = ["[RunAccumulatorMessenger] dumped and reset", "The simulation took"]
failure = ["EEEE", "FatalException"]
benign_warnings = []                  # G4Exception codes that are ignorable

# Outputs
[output]
redirect = { flag = "--dir" }         # how the server isolates a run
default_dir = "<exedir>/results"
manifest = { file = "Manifest.json", kind = "dnachem-manifest", schema = 1 }
dump_prefix_reuse_is_fatal = true

[[output.files]]
glob = "Species_nt_species.csv"; reader = "geant4-csv-ntuple"; role = "gvalues"
columns = { species = "speciesName", time = "time", n_events = "nEvent", sum = "sumG", sum2 = "sumG2" }
time_unit = "ns"
[[output.files]]
glob = "Reactions_nt_reactions.csv"; reader = "geant4-csv-ntuple"; role = "reactions"
metadata = "ReactionsMetadata.csv"

# Convention checks (used by project_check only)
[check]
layout   = "clustered"                # | "flat"
headers  = "header"; sources = "src"; tests = "test"
include_prefix = "cluster"            # includes are "<cluster>/X.hh"
cmake    = "CMakeLists.txt"
[[check.rules]]
id = "portable-class-has-test"; kind = "paired-file"
for = "src/scoring/*.cc"; expect = "test/${stem}Test.cc"; and_cmake = "add_test(NAME ${stem}Test"
```

Rules: unknown keys are errors (typos should not silently disable a limit);
`g4mcp config validate` prints every resolved value and where it came from.

### 7.2 dnachem-min instance (worked example)

This is what would be committed as `dnachem-min/g4mcp.toml`. Every value is taken
from `sim.cc`, `CMakeLists.txt`, `CLAUDE.md`, `.claude/.claude-project.json` or
`docs/output/README.md` as read in this session.

```toml
schema = 1

[project]
name        = "dnachem-min"
kind        = "geant4-dna-app"
description = "Geant4-DNA water radiolysis, SBS / IRT_syn / IRT_syn+mesoscopic"

[geant4]
version       = "11.4.1"
install       = "${G4_ROOT}"
source        = "${DEV_DIR}/GEANT4/geant4-v11.4.1-source/geant4-v11.4.1"
env_script    = "auto"
uses_dna      = true
required_data = ["G4LEDATA"]
# vcpkg HDF5 DLLs and (if the Qt driver is linked) Qt are not on PATH by default;
# env_check proposes the directories, e.g.:
# [run.env] path_prepend = ["${DEV_DIR}/vcpkg/installed/x64-windows/bin"]

[build]
system     = "cmake"
executable = "sim"
[[build.dirs]]
name = "run";  dir = "build";       type = "RelWithDebInfo"; role = "run"
[[build.dirs]]
name = "test"; dir = "build-ninja"; type = "Debug";          role = "test"

[run]
cwd   = "build-dir"
smoke = { macro = "example_irt.in", note = "10 keV e-, /run/beamOn 2; for a quick run issue /scheduler/endTime 1 ms AFTER /run/initialize" }

[run.macro]
dirs            = ["<exedir>", "<exedir>/macro"]     # MacroResolver.cc: arg, exeDir/arg, exeDir/macro/arg
source_dir      = "macro"
arg             = "filename-only"
copied_at_build = true                               # POST_BUILD copy_directory
encoding        = "bytes"
forbidden       = ["/control/shell"]
order_rules = [
  { command = "/scheduler/endTime", after = "/run/initialize",
    why = "ActionInitialization::Build() sets the end time inside /run/initialize" },
  { command = "/chem/select", before = "/run/initialize", why = "PreInit; a different name later is fatal" },
  { command = "/chem/meso/*", before = "/run/initialize", why = "PreInit" },
  { command = "/process/chem/TimeStepModel", before = "/run/initialize", why = "PreInit" },
]

[run.params.threads]
type = "int"; min = 1; max = "cores"; default = 0     # absent = Serial, the project default
channel = "flag"; flag = "--threads"
# note from .claude-project.json: each thread adds a full copy of the chemistry state

[run.params.outdir]
type = "path"; channel = "flag"; flag = "--dir"; auto = "job-outdir"   # parent must exist, leaf is created

[run.params.seed]
type = "int2"; channel = "macro-command"; command = "/random/setSeeds {a} {b}"; anchor = "pre"

[run.limits]
wall_s = 21600; stall_s = 1800; memory_mb = 0; max_concurrent = 1

[run.progress]
inject_print_progress = true
event = '--> Event (\d+) starts'
run   = '### Run (\d+) starts'
done  = '\[RunAccumulatorMessenger\] dumped and reset'

[run.markers]
success = ["[RunAccumulatorMessenger] dumped and reset", "The simulation took"]
failure = ["EEEE", "FatalException"]
benign_warnings = ["WrongResolution"]       # known benign: G4DNAEventScheduler::InitializeInMesh, once per event (CLAUDE.md)

[output]
redirect = { flag = "--dir" }
default_dir = "<exedir>/results"
manifest = { file = "Manifest.json", kind = "dnachem-manifest", schema = 1 }
dump_prefix_reuse_is_fatal = true            # prefix and subdir names; RunAccumulator::TryReserve*
partial_results = "per-dump"                 # data only exists after /run/dumpDataAndReset*; the EndOfRun_ flush runs at normal exit only

[[output.files]]
glob = "*Species_nt_species.csv"; reader = "geant4-csv-ntuple"; role = "gvalues"
columns = { species = "speciesName", time = "time", n_events = "nEvent", sum = "sumG", sum2 = "sumG2" }
time_unit = "ns"
[[output.files]]
glob = "*SpeciesMeso.csv"; reader = "csv"; role = "species-counts-meso"
columns = { time = "time_ns", species = "species", count = "count" }   # rows are summed over events; Txt gives mean per event
[[output.files]]
glob = "*Reactions_nt_reactions.csv"; reader = "geant4-csv-ntuple"; role = "reactions"; metadata = "*ReactionsMetadata.csv"
[[output.files]]
glob = "*TrackLengths.csv"; reader = "csv"; role = "track-lengths"
[[output.files]]
glob = "*PhysicsInteractions.csv"; reader = "csv"; role = "physics-interactions"
[[output.files]]
glob = "*SpeciesMesoSpatial.h5"; reader = "hdf5"; role = "meso-spatial"     # summary only: groups/shapes/attrs
[[output.files]]
glob = "*PreChemical_run*_event*.txt"; reader = "text"; role = "prechemical"; bulk = true   # one per event; never inlined

[check]
layout = "clustered"
headers = "header"; sources = "src"; tests = "test"
include_prefix = "cluster"
cmake = "CMakeLists.txt"          # sources are globbed (CONFIGURE_DEPENDS); only test targets are hand-added
[[check.rules]]
id = "portable-class-has-test"; kind = "paired-file"
for = "src/scoring/*.cc"; expect = "test/${stem}Test.cc"; and_cmake = "add_test(NAME ${stem}Test"
[[check.rules]]
id = "no-generic-messenger-on-shared-dir"; kind = "forbid-pattern"
pattern = 'G4GenericMessenger\([^)]*"/(chem|run|process)/"'      # exact shared roots only; "/chem/env/" (own new directory) does not match
why = "ADR/recipe create-a-macro-command: a second /chem/ directory"
[[check.rules]]
id = "partial-reactions-guarded"; kind = "require-near"
pattern = "SetReactionType\\(1\\)"; require = "PartialReactionsEnabled"   # window/"near" semantics to be tuned against the catalog code
```

Notes on this instance:

- `*Species_nt_species.csv` globs handle `--dir`/prefix/subdir layouts
  (`pfx_Species_nt_species.csv`, `run02/Species_nt_species.csv`), and the server
  reads `Manifest.json` `files` to resolve which dump a file belongs to.
- The `benign_warnings` entry follows the "known benign warning" in `CLAUDE.md`,
  although `.claude-project.json` lists `benignWarnings: []`; resolve which is current
  (open question 8).
- No `[run.macro].prelude` is declared: dnachem-min's macros issue `/run/initialize`
  themselves ("Initialize is performed in beam.in macro" comment in `sim.cc`).
- `sim.cc` accepts a missing macro by defaulting to `beam.in`; the server always
  passes one explicitly.
- Mapping from the existing JSON: `build.*` → `[build]`, `run.cli` → `[run.params]`,
  `run.successMarkers/failureMarkers/benignWarnings` → `[run.markers]`,
  `run.outputs` → `[[output.files]]`, `test.*` → `[tests]` (not shown; the test role
  stays in Claude's existing instructions for now).

### 7.3 How an arbitrary app plugs in

Minimum viable config for an official upstream example (e.g. `chem6`), no changes to
the app:

```toml
schema = 1
[project]   name = "chem6"; kind = "geant4-dna-app"
[geant4]    install = "${G4_ROOT}"; uses_dna = true
[build]     executable = "chem6"
[[build.dirs]] name = "run"; dir = "chem6-build"; type = "Release"; role = "run"
[run]       cwd = "output-dir"            # the app writes Species.txt etc. to cwd
[run.macro] arg = "path"; source_dir = "."; copied_at_build = true
[run.params.threads] type = "int"; channel = "macro-command"; command = "/run/numberOfThreads {v}"; anchor = "pre"
[run.markers] success = ["The simulation took"]
[[output.files]] glob = "Species.txt"; reader = "text"; role = "gvalues-raw"
[[output.files]] glob = "*.root";      reader = "root";  role = "ntuples"      # listed, not parsed (v1)
```

Because `chem6` writes into cwd, `cwd = "output-dir"` makes the server stage each job
in its own directory, so runs never overwrite each other. This is the generic form
of dnachem-min's `--dir`.

For apps with no manifest and ROOT output, the summaries degrade gracefully to
exit classification, log digest and file listing; that is still the high-value part.

---

## 8. Language, SDK, repo

### 8.1 Rust (recommendation)

Reasoning, specific to this project:

- **One self-contained `g4mcp.exe` on Windows.** No Python/Node runtime to keep
  working next to a VS + vcpkg + Geant4 toolchain that is already fragile; easy to
  register as `claude mcp add g4 -- C:\tools\g4mcp.exe serve`.
- **Process control is the core of the job**: Job Objects (kill tree, memory limit,
  `KILL_ON_JOB_CLOSE`), detached supervisors, PE import parsing, `0xC…` exit codes.
  Rust has first-class access (`windows-sys`/`windows`, `goblin`, `sysinfo`).
- **Typed schemas**: tool inputs and the config derive `serde` + `schemars`, so the
  JSON Schema Claude sees cannot drift from the code.
- **Library/binary split.** A `g4-core` crate (config, env discovery, job supervisor,
  output parsers) with no MCP dependency keeps the logic testable and lets a plain CLI
  reuse it later.
- MSVC build tools are already on the machine, which Rust needs for the `*-msvc` target.

Candidate crates: `rmcp` (the official Rust MCP SDK *[verify current API and
version before starting]*), `tokio`, `serde`, `schemars`, `toml`, `csv`, `regex`, `walkdir`, `dunce`, `goblin`, `sysinfo`,
`windows-sys`, `hdf5-metno` or similar (optional), `tracing`.

Costs, honestly: the Rust MCP SDK is younger than the Python/TypeScript ones and may
change; the ecosystem for reading **ROOT** is thin (`oxyroot` exists but is
partial *[verify]*), while Python has `uproot`, `h5py`, `pandas` and the project's
`analysis/` is already Python. Mitigations: (1) pin `rmcp`; the MCP wire format is
simple enough that a ~300-line hand-rolled stdio JSON-RPC layer is a fallback;
(2) keep result *summarisation* behind a trait with an optional `python` sidecar
reader (`uv run reader.py`) for ROOT/HDF5, off by default.

If you would rather prototype in days than weeks, **Python with the official `mcp`
SDK (FastMCP)** is the pragmatic alternative: easier summarisation, harder on Windows
(venv, `pywin32`/`ctypes` for Job Objects). I would still start in Rust because the
supervisor and the PE pre-flight are the parts that must be right, and they are the
parts Python is worst at distributing.

### 8.2 Where it lives

**Its own repo** (`rtonneau/g4mcp`), not inside dnachem-min:

- it is explicitly not dnachem-min-specific; embedding it would bake in DM
  conventions and make upstream-example support an afterthought;
- different toolchain (cargo vs CMake/vcpkg), different release cadence, CI that must
  not need Geant4 (see testing below);
- dnachem-min's own `CLAUDE.md` and `geant4-instructions.md` already treat generic
  Geant4 infrastructure as *reusable across projects*;
- the recipe skill evolves with Geant4 versions, independently of any project.

What stays **in each project** (small, versioned with the code it describes):
`g4mcp.toml` and `docs/recipes/` (the project-specific recipes). Suggested repo layout:

```
g4mcp/
  crates/g4-core/        config, env discovery, job supervisor, readers (no MCP dependency)
  crates/g4mcp/          MCP server binary (serve, supervise, config validate, import-claude-project)
  skills/geant4-dna-recipes/   cross-project recipe skill (section 5)
  examples/              g4mcp.toml for dnachem-min, chem6, UHDR, dnaphysics
  tests/fixtures/        fake Geant4 apps and canned logs/outputs (no Geant4 needed)
```

Registration: user scope once, e.g.
`claude mcp add --scope user g4 -- C:\tools\g4mcp\g4mcp.exe serve`; per-project
discovery by `g4mcp.toml` found from the client's working directory, plus a global
list of extra roots in `%APPDATA%\g4mcp\config.toml`.

### 8.3 Security and trust model (small but not optional)

- Executes only executables resolved from a registered project's `[build]`
  (allow-list), via argv arrays, never a shell string.
- Macro text is Claude-authored data: `forbidden` commands (`/control/shell`) are
  refused at `macro_validate`/`run_start`; `/control/execute` targets are confined to
  project macro dirs and the job dir.
- Output paths confined to the job dir / declared output dirs; `..` rejected.
- Logs, manifests and macros returned to Claude are **untrusted data** (a log line can
  say anything); tool results label them as such and never get executed.
- No network access.

### 8.4 Testing without Geant4

A tiny **fake app** (Rust bin in `tests/fixtures`) that prints Geant4-like lines
(`### Run 0 starts.`, `--> Event 3 starts.`, a `G4Exception` block), exits with
chosen codes (including `0xC0000135` simulated), sleeps, writes CSV/JSON, and grows
memory on request. All supervisor, classifier and reader logic is tested against
it. Integration test (manual, Windows): dnachem-min `example_irt.in` smoke run.

---

## 9. Queueing and concurrency

v1 has no queue. `run_start` checks `max_concurrent` and available memory (admission);
if no slot is free it fails with `no_slot` and a list of running jobs, or waits when
`wait_for_slot` is set (bounded by `wait_timeout_s`). Jobs are independent directories,
so the server stays stateless.

A real queue (ordering, priorities, retry after `oom_killed`, a batch of parameter
scans) is a later addition built on the same job directories: a `queued` state, a
dispatcher in the server that starts the next job when a slot frees, and a
`queue.json` beside the jobs. It is deliberately not designed further until the
single-run path works (open question 2).

---

## 10. Milestones

| M | Content | Done when |
|---|---|---|
| 0 | repo, CI, config parser + `config validate`, `import-claude-project`, fake app fixtures | dnachem-min `g4mcp.toml` validates and round-trips |
| 1 | `project_*`, `builds_find`, `env_check` (data sets, PE pre-flight), `ui_commands` | `env_check` reproduces the `0xC0000135` diagnosis on a deliberately broken PATH |
| 2 | supervisor, `run_prepare/start/status/wait/logs/cancel/list/clean`, classifier | dnachem-min `example_irt.in` smoke runs from Claude, cancel kills the tree, server restart keeps the job |
| 3 | readers + `results_*`, `gvalues`, `run_summary` using `Manifest.json` | G-value table for a smoke run matches the CSV by hand |
| 4 | `macro_validate`, overlay, aliases, order rules | a macro with `/scheduler/endTime` before `/run/initialize` is flagged for DM |
| 5 | recipe skill `geant4-dna-recipes` (messenger, counter, scorer, chemistry first), `CLAUDE.md` pointer in dnachem-min | Claude, given "add a counter" and the recipe, produces the edits the DM recipe lists; a new command shows up in `ui_commands` |
| 6 | `project_check` with three rules (paired test, shared-dir messenger, guarded `SetReactionType`) | rules fire on a deliberately broken copy and are silent on dnachem-min |
| 7 | admission/queue, `runs_compare`, HDF5 summary, optional ROOT sidecar | decided after M2 (open question 2) |

Verification of each milestone is by running it, not by reading the code: the
M2/M3 checks use the real DM smoke macro; M5/M6 by trying the recipes on a real
change (compiled on the user's machine; this environment cannot build).

---

## 11. Risks

| Risk | Impact | Mitigation |
|---|---|---|
| Killing a run loses all data (DM writes only at dump) | wasted hours after a cancel/timeout | recommend per-sub-run dumps in macros; `partial_results` in config; `run_cancel` reports what exists; consider an app-side "stop file" polled between events (needs app change; open question 4) |
| Text-level macro overlay is fragile (`after_init` anchors, order rules) | silently wrong run | refuse when anchor missing; always save `macro.final.in` + diff; encourage aliases in new macros |
| Windows process model (no graceful interrupt, orphans, handle inheritance, Job Object breakaway) | stuck or orphaned `sim.exe` | supervisor owns a Job Object with kill-on-close; detach via `CREATE_NEW_PROCESS_GROUP` and breakaway; heartbeat + `lost` state; test with kill -9 of server and supervisor |
| Memory: MT multiplies chemistry memory; host kills jobs under pressure | OOM or host instability | per-job memory limit via Job Object; admission control; Serial default; user fills `memory_per_unit_mb` after measuring |
| PE pre-flight false positives/negatives (delay-load DLLs, API sets, manifests) | misleading advice | report as "likely"; the real test is `ui_commands` (cheap app launch); never block on pre-flight alone |
| Debug/Release mix with Geant4 install | memory corruption (seen once in DM tests) | record build type per run, warn on mixes, default `role=run` build |
| `ui_commands` runs the app: apps that block on interactive UI or do heavy init | hang | generated batch macro, short timeout, only if config says `batch_safe = true` |
| Parsing Geant4 log text and Geant4 CSV/ROOT formats across versions | brittle summaries | patterns in config, reader unit tests with captured real files per Geant4 version, fail with "unrecognised" not wrong numbers |
| Recipes drift from the code or the Geant4 version | Claude follows stale advice | each recipe cites file paths and the Geant4 version; `project_check` rules catch the worst pitfalls; update recipes in the same PR as the code they describe |
| Scope creep into building / analysis / plotting | never ships | v1 = run + summarise + two checks; build, plots, ROOT are explicit non-goals |
| Rust MCP SDK churn | rework | pin; thin adapter layer; fallback hand-rolled JSON-RPC |
| Prompt-injection via logs/macros/manifests | tool misuse | treat as data, no auto-follow, allow-list exes and commands |
| Non-reproducibility (chemistry stepping diverges run-to-run even with the same seed; `sim.cc` comment, ADR 0005) | users expect replay | record seed and the full resolved request; `runs_compare` states when seeds/threads differ; never claim reproducibility |
| License/attribution when recipes quote upstream code | compliance | quote short snippets with a source path, keep the Geant4 citation requirement in mind for code derived from examples |

---

## 12. Open questions for you

1. **Name and repo**: `g4mcp` under `rtonneau/`? Public or private?
2. **Queueing**: do you need a queue (parameter scans, overnight batches, retry after
   OOM) in the first release, or is admission control with `max_concurrent` enough
   until the single-run path works (section 9)?
3. **Config file**: OK with a new `g4mcp.toml`, or should I read `.claude/.claude-project.json`
   as the primary config and extend it? (It already has most of the keys, supports
   `${VAR}` expansion, and Claude's `CONFIG_RESOLUTION.md` apparently defines path
   resolution I have not seen.)
4. **Cancel semantics**: can I add an app-side convention to dnachem-min (a stop file
   polled at end of event, or a signal handler that triggers an `EndOfRun_` flush),
   so cancel and timeout do not lose a long run? Otherwise the answer is "dump per
   sub-run in the macro".
5. **Recipe skill**: should the cross-project skill live in the `g4mcp` repo, in your
   dotfiles skills, or both? Should the project-specific recipes in `docs/recipes/`
   be linked from `CLAUDE.md` with an explicit "read before adding X" rule?
6. **ROOT**: do you read ROOT outputs from the upstream examples? If yes, are you
   willing to accept a Python sidecar (`uproot`) or should the plan restrict itself to
   CSV/HDF5/XML and push users to `SetDefaultFileType("csv")`?
7. **Who runs the server**: always on this one Windows machine, or also a Linux
   box/cluster login node? That decides how much Linux, SSH and batch-scheduler
   (Slurm) support belongs in v1 rather than "cheap".
8. **Benign warnings for DM**: `CLAUDE.md` lists the `WrongResolution` warning as known
   and benign, `.claude-project.json` has `benignWarnings: []`. Which is current?
9. **G-value definition**: confirm `sumG`/`sumG2`/`nEvent` semantics and units for
   `Species_nt_species.csv` and whether the server may compute `G = sumG/nEvent` with
   an SEM from `sumG2`.
10. **Allowed to run debug builds?** Should `role=test` (Debug) builds be allowed for
    simulations at all, given the slowness and the build-type mix risk?
11. **Concurrency policy**: how many simultaneous runs on this machine, and what memory
    budget per run, so `max_concurrent` and admission have real defaults?
12. **Pre-flight depth**: should `env_check` also launch the exe once with a trivial
    macro (`/control/manual /`) by default, or only on request?
13. **Scope of upstream coverage in v1**: which of `chem6`, `UHDR`, `scavenger`,
    `dnaphysics` do you actually run, so the example configs and fixtures target them
    first?
14. **G4Vox**: you said dnachem-min depends on the local G4Vox library, but nothing
    in `CMakeLists.txt` links it and `CLAUDE.md` forbids G4Vox-based geometry here.
    Which of your projects do link it? Should the config support such an external
    local library (extra include/link/DLL dirs) so `env_check` and `path_prepend`
    cover it, or is that out of scope?
15. **Command roots in recipes**: for new projects, default to a project-owned
    command root (`/<app>/...`) as upstream's `UHDR` does, instead of piggy-backing on
    `/chem/` and `/run/` as DM does? I suggest yes, with a warning on shared roots.
