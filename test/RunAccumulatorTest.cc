/// \file RunAccumulatorTest.cc
/// \brief Plain-assert unit tests for RunAccumulator (no test framework, no
/// Geant4 runtime -- exercises pure accumulation/prefix-bookkeeping logic
/// only. DumpAndReset's file-writing/species lookup lives in
/// RunAccumulatorMessenger and is integration-verified via sim.exe instead.)

#include "RunAccumulator.hh"

#include "G4SystemOfUnits.hh"

#include <cassert>
#include <iostream>

// --- Accumulate / HasPendingData / GetAccumulated* -------------------------

static void TestAccumulateSetsPendingFlag()
{
  RunAccumulator::ClearAccumulated();
  assert(!RunAccumulator::HasPendingData());

  ReactionCounter reactions;
  PhysicsInteractionCounter interactions;
  RunAccumulator::Accumulate(1 * CLHEP::keV, reactions, interactions);

  assert(RunAccumulator::HasPendingData());

  RunAccumulator::ClearAccumulated();
}

static void TestAccumulateSumsEnergyAcrossCalls()
{
  RunAccumulator::ClearAccumulated();

  ReactionCounter reactions;
  PhysicsInteractionCounter interactions;
  RunAccumulator::Accumulate(1 * CLHEP::keV, reactions, interactions);
  RunAccumulator::Accumulate(2 * CLHEP::keV, reactions, interactions);

  assert(RunAccumulator::GetAccumulatedEnergy() == 3 * CLHEP::keV);

  RunAccumulator::ClearAccumulated();
}

static void TestAccumulateMergesReactionCounts()
{
  RunAccumulator::ClearAccumulated();

  ReactionCounter reactions;
  reactions.Record("H + H -> H2", 1 * CLHEP::picosecond);
  PhysicsInteractionCounter interactions;

  RunAccumulator::Accumulate(0., reactions, interactions);
  RunAccumulator::Accumulate(0., reactions, interactions);

  assert(RunAccumulator::GetAccumulatedReactionCounter()
             .GetCounts().at(1 * CLHEP::picosecond).at("H + H -> H2") == 2);

  RunAccumulator::ClearAccumulated();
}

static void TestAccumulateMergesInteractionCounts()
{
  RunAccumulator::ClearAccumulated();

  ReactionCounter reactions;
  PhysicsInteractionCounter interactions;
  interactions.Record("e-_G4DNAIonisation");

  RunAccumulator::Accumulate(0., reactions, interactions);
  RunAccumulator::Accumulate(0., reactions, interactions);

  assert(RunAccumulator::GetAccumulatedInteractionCounter()
             .GetCounts().at("e-_G4DNAIonisation") == 2);

  RunAccumulator::ClearAccumulated();
}

static void TestAccumulateDoesNotModifyItsInputs()
{
  RunAccumulator::ClearAccumulated();

  ReactionCounter reactions;
  reactions.Record("H + H -> H2", 1 * CLHEP::picosecond);
  PhysicsInteractionCounter interactions;
  interactions.Record("e-_G4DNAIonisation");

  RunAccumulator::Accumulate(0., reactions, interactions);

  assert(reactions.GetCounts().at(1 * CLHEP::picosecond).at("H + H -> H2") == 1);
  assert(interactions.GetCounts().at("e-_G4DNAIonisation") == 1);

  RunAccumulator::ClearAccumulated();
}

// --- ClearAccumulated -------------------------------------------------

static void TestClearAccumulatedResetsEverything()
{
  ReactionCounter reactions;
  reactions.Record("H + H -> H2", 1 * CLHEP::picosecond);
  PhysicsInteractionCounter interactions;
  interactions.Record("e-_G4DNAIonisation");
  RunAccumulator::Accumulate(5 * CLHEP::keV, reactions, interactions);

  RunAccumulator::ClearAccumulated();

  assert(!RunAccumulator::HasPendingData());
  assert(RunAccumulator::GetAccumulatedEnergy() == 0.);
  assert(RunAccumulator::GetAccumulatedReactionCounter().GetCounts().empty());
  assert(RunAccumulator::GetAccumulatedInteractionCounter().GetCounts().empty());
}

// --- TryReservePrefix --------------------------------------------------

static void TestTryReservePrefixAcceptsFirstUse()
{
  G4String err;
  assert(RunAccumulator::TryReservePrefix("ticket02_first_", true, err));
  assert(err.empty());
}

static void TestTryReservePrefixRefusesRepeatWhenEnforced()
{
  G4String err;
  assert(RunAccumulator::TryReservePrefix("ticket02_repeat_", true, err));

  G4String err2;
  assert(!RunAccumulator::TryReservePrefix("ticket02_repeat_", true, err2));
  assert(!err2.empty());
}

static void TestTryReservePrefixAllowsRepeatWhenNotEnforced()
{
  G4String err;
  assert(RunAccumulator::TryReservePrefix("ticket02_unenforced_", true, err));

  G4String err2;
  assert(RunAccumulator::TryReservePrefix("ticket02_unenforced_", false, err2));
  assert(err2.empty());
}

static void TestTryReservePrefixTreatsEmptyPrefixAsReservable()
{
  G4String err;
  assert(RunAccumulator::TryReservePrefix("", true, err));

  G4String err2;
  assert(!RunAccumulator::TryReservePrefix("", true, err2));
}

int main()
{
  TestAccumulateSetsPendingFlag();
  TestAccumulateSumsEnergyAcrossCalls();
  TestAccumulateMergesReactionCounts();
  TestAccumulateMergesInteractionCounts();
  TestAccumulateDoesNotModifyItsInputs();

  TestClearAccumulatedResetsEverything();

  TestTryReservePrefixAcceptsFirstUse();
  TestTryReservePrefixRefusesRepeatWhenEnforced();
  TestTryReservePrefixAllowsRepeatWhenNotEnforced();
  TestTryReservePrefixTreatsEmptyPrefixAsReservable();

  std::cout << "All RunAccumulator tests passed." << std::endl;
  return 0;
}
