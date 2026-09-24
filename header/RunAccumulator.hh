/// \file RunAccumulator.hh
/// \brief Process-wide accumulator for data that must survive across
/// /run/beamOn calls until explicitly flushed.
///
/// Species yields don't need this -- the ScoreSpecies scorer is itself a
/// persistent, SD-registered object and accumulates on its own. Energy
/// deposit and the two counters (ReactionCounter, PhysicsInteractionCounter)
/// live on the per-run Run object today, which is destroyed every beamOn --
/// RunAccumulator gives them a persistent home instead, fed once per run
/// from RunAction::EndOfRunAction.
///
/// Pure accumulation/bookkeeping: no file I/O, no Geant4 kernel dependency
/// beyond G4double/G4String. RunAccumulatorMessenger owns the file-writing
/// side (species lookup, OutputDir, G4AnalysisManager) and the public
/// UI-command / exit-time entry points.

#ifndef RunAccumulator_h
#define RunAccumulator_h 1

#include "ReactionCounter.hh"
#include "PhysicsInteractionCounter.hh"

#include "globals.hh"

namespace RunAccumulator
{
  /// Merges energy/reactions/interactions into the persistent totals
  /// (reactions and interactions are merged via ReactionCounter::Merge /
  /// PhysicsInteractionCounter::Merge -- their arguments are left
  /// unmodified) and marks pending data. Called once per run from
  /// RunAction::EndOfRunAction (master thread only).
  void Accumulate(G4double energy, const ReactionCounter &reactions,
                   const PhysicsInteractionCounter &interactions);

  /// True if Accumulate() has added data since the last ClearAccumulated().
  G4bool HasPendingData();

  G4double GetAccumulatedEnergy();
  const ReactionCounter &GetAccumulatedReactionCounter();
  const PhysicsInteractionCounter &GetAccumulatedInteractionCounter();

  /// Resets energy to 0 and both counters to empty, and clears the
  /// pending-data flag. Does not touch the prefix-uniqueness set.
  void ClearAccumulated();

  /// If enforceUniqueness is false, or prefix hasn't been reserved before,
  /// records prefix as used and returns true. If enforceUniqueness is true
  /// and prefix was already reserved earlier in this process, leaves state
  /// unchanged, sets err, and returns false.
  G4bool TryReservePrefix(const G4String &prefix, G4bool enforceUniqueness,
                           G4String &err);

  /// Same contract as TryReservePrefix, for dump subfolder names. Kept in
  /// a separate set: a prefix and a subfolder never collide with each other.
  G4bool TryReserveSubdir(const G4String &subdir, G4bool enforceUniqueness,
                           G4String &err);
}

#endif // RunAccumulator_h
