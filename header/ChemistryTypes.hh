/// \file ChemistryTypes.hh
/// \brief Plain data types shared by every Chemistry file. Standard library only.
#ifndef ChemistryTypes_h
#define ChemistryTypes_h 1

#include <string>
#include <vector>

namespace ChemistryTypes
{
  /// One first-order or pseudo-first-order acid-base reaction of a tracked
  /// molecule against a partner (a bulk species such as "H3Op(B)" or "H2O").
  /// rate is already in Geant4 internal units. reactionType 0 = leave unset.
  struct AcidBaseReaction
  {
    std::string partner;
    double rate;
    std::vector<std::string> products;
    int reactionType = 0;
  };

  /// All acid-base reactions registered as one G4DNAScavengerProcess on `molecule`.
  struct AcidBaseEntry
  {
    std::string molecule;
    std::vector<AcidBaseReaction> reactions;
  };

  /// May be empty: a Chemistry without the acid-base buffer.
  using AcidBaseList = std::vector<AcidBaseEntry>;
}  // namespace ChemistryTypes

#endif  // ChemistryTypes_h
