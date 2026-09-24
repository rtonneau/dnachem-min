/// \file RunAccumulator.cc
/// \brief Implementation of RunAccumulator

#include "RunAccumulator.hh"

#include <set>

namespace
{
  G4double gAccumulatedEnergy = 0.;
  ReactionCounter gAccumulatedReactionCounter;
  PhysicsInteractionCounter gAccumulatedInteractionCounter;
  G4bool gHasPendingData = false;
  std::set<G4String> gUsedPrefixes;
}

void RunAccumulator::Accumulate(G4double energy, const ReactionCounter &reactions,
                                 const PhysicsInteractionCounter &interactions)
{
  gAccumulatedEnergy += energy;
  gAccumulatedReactionCounter.Merge(reactions);
  gAccumulatedInteractionCounter.Merge(interactions);
  gHasPendingData = true;
}

G4bool RunAccumulator::HasPendingData()
{
  return gHasPendingData;
}

G4double RunAccumulator::GetAccumulatedEnergy()
{
  return gAccumulatedEnergy;
}

const ReactionCounter &RunAccumulator::GetAccumulatedReactionCounter()
{
  return gAccumulatedReactionCounter;
}

const PhysicsInteractionCounter &RunAccumulator::GetAccumulatedInteractionCounter()
{
  return gAccumulatedInteractionCounter;
}

void RunAccumulator::ClearAccumulated()
{
  gAccumulatedEnergy = 0.;
  gAccumulatedReactionCounter.Clear();
  gAccumulatedInteractionCounter.Clear();
  gHasPendingData = false;
}

G4bool RunAccumulator::TryReservePrefix(const G4String &prefix, G4bool enforceUniqueness,
                                         G4String &err)
{
  if (enforceUniqueness && gUsedPrefixes.count(prefix) > 0)
  {
    err = "prefix '" + prefix + "' was already used earlier in this run -- choose a different prefix";
    return false;
  }

  gUsedPrefixes.insert(prefix);
  return true;
}
