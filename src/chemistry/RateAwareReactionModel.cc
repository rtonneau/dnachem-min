/// \file RateAwareReactionModel.cc
/// \brief Implementation of RateAwareReactionModel. See the header and
///        docs/adr/0006-opt-in-sbs-rate-aware-acceptance.md.

#include "chemistry/RateAwareReactionModel.hh"

#include "chemistry/EncounterProbability.hh"

#include "G4Molecule.hh"
#include "G4Step.hh"
#include "G4Track.hh"
#include "Randomize.hh"

#include <cmath>

G4bool RateAwareReactionModel::FindReaction(const G4Track& trackA, const G4Track& trackB,
                                            G4double reactionRadius,
                                            G4double& separationDistance,
                                            G4bool alongStepReaction)
{
  // In-radius test, identical to G4DNASmoluchowskiReactionModel::FindReaction:
  // accumulate the squared separation axis by axis and stop as soon as it
  // exceeds R^2.
  const G4double R2 = reactionRadius * reactionRadius;
  G4double postStepSeparation = 0.;
  G4bool outsideRadius = false;
  int k = 0;

  for (; k < 3; ++k) {
    postStepSeparation += std::pow(trackA.GetPosition()[k] - trackB.GetPosition()[k], 2);
    if (postStepSeparation > R2) {
      outsideRadius = true;
      break;
    }
  }

  if (!outsideRadius) {
    separationDistance = std::sqrt(postStepSeparation);
    return true;
  }

  if (alongStepReaction) {
    for (++k; k < 3; ++k) {
      postStepSeparation += std::pow(trackA.GetPosition()[k] - trackB.GetPosition()[k], 2);
    }
    separationDistance = (postStepSeparation = std::sqrt(postStepSeparation));

    const G4double diffusionSum = GetMolecule(trackA)->GetDiffusionCoefficient()
                                  + GetMolecule(trackB)->GetDiffusionCoefficient();

    const auto& preStepPositionA = trackA.GetStep()->GetPreStepPoint()->GetPosition();
    const auto& preStepPositionB = trackB.GetStep()->GetPreStepPoint()->GetPosition();
    const G4double preStepSeparation = (preStepPositionA - preStepPositionB).mag();

    // Exact encounter probability of the radial bridge (Geant4 uses only its
    // large-separation limit).
    const G4double probabilityOfEncounter =
      EncounterProbability(preStepSeparation, postStepSeparation, reactionRadius, diffusionSum,
                           trackB.GetStep()->GetDeltaTime());

    if (G4UniformRand() <= probabilityOfEncounter) {
      return true;
    }
  }

  return false;
}
