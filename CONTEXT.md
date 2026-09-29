# dnachem-min

Minimal Geant4-DNA water-radiolysis chemistry. This glossary tracks the chemistry
vocabulary used in `DnaChemistryList`/`DnaChemistryWorld` and related design
discussions; it is not a spec or implementation log.

## Language

**Chemistry**:
A named, selectable definition of the reaction network (including its bulk
reactions: acid-base buffer and scavenger rates), chosen once per process before initialization. Names are PascalCase
and matched case-insensitively (`PureWater`, `BoscoloChem`, ...). All Chemistries
share the same molecule set and dissociation channels. `PureWater` is the default.
A Chemistry reproduces the network of a given paper or library; it is a variant of
the reaction content only, not of the time-step model, physics, or scoring.
_Avoid_: "chemistry model" (collides with the chemistry time-step model, e.g. SBS);
"chemistry list" (that is the Geant4 class `G4VUserChemistryList`, the driver).

**Pure-water chemistry** (the `PureWater` Chemistry, baseline chemistry):
The default Chemistry's reaction network: the diffusion-controlled radical reactions
between tracked molecules (e_aq, H, °OH, H2, H2O2, O2, O2⁻, HO2°, HO2⁻, O⁻, O3⁻...),
plus the pH-dependent acid-base buffer equilibria against the bulk `H3Op(B)`/`OHm(B)`
species. Active regardless of whether dissolved O2 is present — the network can
produce O2 purely from water radiolysis (e.g. H2O2 + °OH → HO2° → [pH equilibrium]
→ O2⁻, then O2⁻ + °OH → O2).
_Avoid_: "water chemistry" alone (ambiguous with the O2-scavenger addition below).

**Scavenger**:
An *exogenous* dissolved species present in the sample as a bulk component (e.g.
atmospheric O2, set via `/chem/env/scavenger O2 21 %`), acting as an additional
reactant/sink for tracked molecules. Its concentration is part of the environment
(shared by all Chemistries); the reactions it takes part in, and their rates, are
**Bulk reactions** of the selected Chemistry. A concentration of 0 means the
scavenger is absent. Consumed as it reacts, restored to its initial concentration at
the start of each event's chemistry. Not required for pure-water chemistry to
function or to produce O2 — see [[0001-baseline-acid-base-buffer]].
_Avoid_: using "scavenger" for the pH acid-base buffer network itself — that is
baseline pure-water chemistry, not a scavenger.

**Bulk species** (bulk component):
A homogeneous background concentration rather than an individually tracked,
diffusing molecule: the pH-owned pseudo-species (name suffix `(B)`, e.g. `H3Op(B)`,
`OHm(B)`), bulk water, and any **Scavenger** (which, unlike the pH pool, is keyed on
the ordinary species name, e.g. `O2` — the same species can exist both as tracked
radiolytic molecules and as a bulk background). Reactions against a bulk species
only fire as **Bulk reactions** — an ordinary reaction-table entry naming a bulk
species reacts only with tracked molecules of that name, never with the background.

**Bulk reaction**:
A reaction of a tracked molecule with a bulk species, defined per molecule by the
selected Chemistry: the acid-base buffer equilibria (against `H3Op(B)`/`OHm(B)`/
water) and the scavenger reactions (e.g. e_aq + O2(bulk) → O2⁻). Distinct from the
bimolecular reactions between two tracked molecules.
_Avoid_: "acid-base reaction" as the umbrella term — acid-base is only one kind of
bulk reaction.

**Dump**:
One flush of everything accumulated since the previous flush (or program start) into
output files, then a reset — issued by `/run/dumpDataAndReset` or
`/run/dumpDataAndResetToDir`, or by the automatic exit-time flush. A dump is the unit
of "one simulation" for output purposes: it may cover several `/run/beamOn` calls.
_Avoid_: "run" or "simulation" for this unit — a Geant4 run is a single `/run/beamOn`.

**Manifest**:
The structured description of one **Dump**, written next to the data files it
describes: what was simulated (beam per run, Chemistry, environment), how (seed,
threads), the totals (events, energy deposit) and which files the dump produced.
Every dump has exactly one.
_Avoid_: "metadata" (already names `ReactionsMetadata.csv`, the reaction-id → label map).
