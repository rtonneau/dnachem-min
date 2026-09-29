/// \file ChemistryTypes.hh
/// \brief Plain data types shared by every Chemistry file. Standard library only.
#ifndef ChemistryTypes_h
#define ChemistryTypes_h 1

#include <string>
#include <vector>

namespace ChemistryTypes
{
  /// One first-order or pseudo-first-order reaction of a tracked molecule
  /// against a bulk species: an acid-base buffer partner ("H3Op(B)",
  /// "OHm(B)", "H2O") or a scavenger ("O2"). rate is already in Geant4
  /// internal units. reactionType 0 = leave unset.
  struct BulkReaction
  {
    std::string partner;
    double rate;
    std::vector<std::string> products;
    int reactionType = 0;
  };

  /// All bulk reactions registered as one G4DNAScavengerProcess on `molecule`
  /// (Geant4 allows one such process per molecule).
  struct BulkReactionEntry
  {
    std::string molecule;
    std::vector<BulkReaction> reactions;
  };

  /// May be empty: a Chemistry without bulk reactions.
  using BulkReactionList = std::vector<BulkReactionEntry>;
}  // namespace ChemistryTypes

#endif  // ChemistryTypes_h
