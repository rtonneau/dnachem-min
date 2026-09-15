# dnachem-min

Minimal Geant4-DNA water-radiolysis chemistry. This glossary tracks the chemistry
vocabulary used in `DnaChemistryList`/`DnaChemistryWorld` and related design
discussions; it is not a spec or implementation log.

## Language

**Pure-water chemistry** (baseline chemistry):
The always-active reaction network: the diffusion-controlled radical reactions
between tracked molecules (e_aq, H, °OH, H2, H2O2, O2, O2⁻, HO2°, HO2⁻, O⁻, O3⁻...),
plus the pH-dependent acid-base buffer equilibria against the bulk `H3Op(B)`/`OHm(B)`
species. Active regardless of whether dissolved O2 is present — the network can
produce O2 purely from water radiolysis (e.g. H2O2 + °OH → HO2° → [pH equilibrium]
→ O2⁻, then O2⁻ + °OH → O2).
_Avoid_: "water chemistry" alone (ambiguous with the O2-scavenger addition below).

**Scavenger**:
A mechanism for introducing an *exogenous* dissolved species (e.g. atmospheric O2
dissolved in the sample, set via `/chem/env/O2`) as an additional reactant/sink,
layered on top of pure-water chemistry. Not required for pure-water chemistry to
function or to produce O2 — see [[0001-baseline-acid-base-buffer]].
_Avoid_: using "scavenger" for the pH acid-base buffer network itself — that is
baseline pure-water chemistry, not a scavenger.

**Bulk species**:
A pseudo-species (name suffix `(B)`, e.g. `H3Op(B)`, `OHm(B)`) representing a
homogeneous background concentration (e.g. the solution's pH-buffered H3O+/OH-
pool) rather than an individually tracked, diffusing molecule. Reactions against a
bulk species only fire when a `G4DNAScavengerProcess` is registered for the real
molecule on the other side — an ordinary reaction-table entry naming a bulk species
is otherwise inert (no tracks exist to encounter it).
