/// \file RateAwareReactionModel.hh
/// \brief SBS reaction model whose along-step (Brownian-bridge) acceptance
///        uses the exact 3D encounter probability.
///
/// Same as G4DNASmoluchowskiReactionModel (Geant4 11.4.1,
/// G4DNASmoluchowskiReactionModel.cc:63-133) except for the in-between test:
/// Geant4 accepts with exp(-(r1-R)(r2-R)/(D dt)), which does not vanish as
/// R -> 0, so slow reactions fire about as often as diffusion-controlled ones.
/// This model draws against EncounterProbability(r1, r2, R, D, dt) instead,
/// which -> 0 as R -> 0 and -> the Geant4 value when R(r1+r2) >> D dt.
/// R is still Geant4's effective radius k/(4 pi D N_A).
///
/// Opt-in: /chem/sbs/rateAwareReactions true (DnaChemistryList).
/// See docs/adr/0006-opt-in-sbs-rate-aware-acceptance.md.
///
/// Depends only on Geant4 and EncounterProbability; include prefix is rooted
/// at header/, adjust it on copy.

#ifndef RateAwareReactionModel_hh
#define RateAwareReactionModel_hh 1

#include "G4DNASmoluchowskiReactionModel.hh"

class RateAwareReactionModel : public G4DNASmoluchowskiReactionModel
{
public:
  RateAwareReactionModel() = default;
  ~RateAwareReactionModel() override = default;

  RateAwareReactionModel(const RateAwareReactionModel&) = delete;
  RateAwareReactionModel& operator=(const RateAwareReactionModel&) = delete;

  /// In-radius test and separation output as in Geant4; along-step test with
  /// the exact encounter probability.
  G4bool FindReaction(const G4Track& trackA, const G4Track& trackB, G4double reactionRadius,
                      G4double& separationDistance, G4bool alongStepReaction) override;
};

#endif
