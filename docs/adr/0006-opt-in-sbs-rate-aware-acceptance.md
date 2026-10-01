---
status: accepted
---

# Opt-in rate-aware reaction acceptance for SBS

`/chem/sbs/rateAwareReactions true` (PreInit, default `false`) replaces the reaction model of the step-by-step (SBS) time-step model with `RateAwareReactionModel`. It keeps Geant4's in-radius test and changes only the along-step test, which decides whether two molecules that are outside the reaction radius at both ends of a step met during it. The switch is recorded in `Manifest.json` as `sbs.rateAwareReactions`.

## The artifact

Geant4 gives every reaction an effective radius R = k / (4π D N_A) (`G4DNAMolecularReactionData::ComputeEffectiveRadius`, D = D_A + D_B), and `G4DNASmoluchowskiReactionModel::FindReaction` (Geant4 11.4.1, `G4DNASmoluchowskiReactionModel.cc:119`) accepts a pair that is outside R at both ends of a step with the Brownian-bridge probability

    P_G4 = exp(-(r1 - R)(r2 - R) / (D Δt))

where r1 and r2 are the pre- and post-step separations. P_G4 does not vanish as R → 0: once steps are long compared with r1·r2/D, slow reactions fire about as often as diffusion-controlled ones. Under `BoscoloChem` (100 keV electrons, 2 events, scratch runs at pO2 0% and 21%, `.scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/runs/`), the slow Table 1 reactions OH + H2 → H (k = 4.5e7 M⁻¹s⁻¹), OH + H2O2 → HO2° (2.3e7), H + OH⁻ → e_aq (2e7) and H + H2O2 → OH (1e8) fired at diffusion-controlled frequencies at late times: between 30 and 100 ns at 0%, OH + H2 fired 66 times against 77 for OH + H, whose rate constant (2.0e10) is more than 400 times higher. This is what makes H• rise again after 1 ns and HO2° keep rising at 1 µs, against Boscolo et al. 2020, Fig. 3.

## The formula

The pair separation performs 3D Brownian motion with diffusion sum D. With u = r·p the radial process becomes one-dimensional, an absorbing sphere at R is a Dirichlet condition at u = R, and the method of images gives the exact probability that the bridge from r1 to r2 in Δt has touched R:

    P = [exp(-(r1 - R)(r2 - R) / (D Δt)) - exp(-r1 r2 / (D Δt))] / [1 - exp(-r1 r2 / (D Δt))]

P → 0 as R → 0, and P → P_G4 when R(r1 + r2) >> D Δt, so diffusion-controlled reactions are unchanged and, over long times, a pair reacts at the Smoluchowski rate 4π D R N_A = k. Geant4's R is kept: no contact radius is needed. The formula lives in the kernel-free `EncounterProbability` (`src/chemistry/EncounterProbability.cc`, unit-tested in `test/EncounterProbabilityTest.cc`). `RateAwareReactionModel::FindReaction` draws `G4UniformRand() <= EncounterProbability(r1, r2, R, D_A + D_B, Δt)` with Δt = `trackB.GetStep()->GetDeltaTime()`, as Geant4 does.

The model also fixes a Geant4 bookkeeping slip in the same function: when the in-radius loop breaks at axis k, Geant4's continuation loop starts again at k and adds that axis's squared difference twice, which overstates r2. `RateAwareReactionModel` resumes at k + 1, so r2 is the true post-step separation. The in-radius result is identical.

Geant4 11.4.1 has no `G4DNAMolecularStepByStepModel` constructor taking a reaction model. `DnaChemistryList::ConstructTimeStepModel` calls `SetReactionModel(new RateAwareReactionModel())` on a default-constructed SBS model; `Initialize()` keeps a model that is already set (`G4DNAMolecularStepByStepModel.cc:69-74`), and the SBS model owns it.

## Opt-in rather than global

Off, `ConstructTimeStepModel` still registers exactly `new G4DNAMolecularStepByStepModel()`, so `PureWater` baselines and every earlier output stay comparable. The switch is not tied to a Chemistry: per [[0002-named-chemistries]] a Chemistry varies reaction content only, not the time-step model. The cost is two acceptance paths in the code and a manifest field that must be read to interpret a run.

## Rejected alternatives

- **Step cap alone** (`G4Scheduler::SetMaxTimeStep`): shorter steps shrink D Δt and so P_G4, but slow reactions are still accepted at a rate set by the step length, not by k, and a cap small enough to suppress them costs far too much runtime. The cap is kept as a separate, independent switch for the end-of-chemistry step burst.
- **No bridge, as in TRAX**: accepting only pairs found inside R at the end of a step underestimates diffusion-controlled reactions whenever the step is long, which is exactly when the bridge matters.
- **Contact radius plus a per-encounter reaction probability** (partially diffusion-controlled model): it needs a contact distance per reaction pair that Table 1 does not give, and it changes Geant4's radii. The exact formula reaches the right long-time rate with Geant4's R.

**Consequences**: a run with the switch on uses a different random-number sequence from one without it, so the two are only statistically comparable. The switch is per process (PreInit) and applies to all thread-local SBS models, since they read it from the shared `DnaChemistryList`.
