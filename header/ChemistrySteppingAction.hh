#ifndef ChemistrySteppingAction_hh
#define ChemistrySteppingAction_hh 1

#include "G4Types.hh"
#include "G4UserSteppingAction.hh"

class ChemistrySteppingAction : public G4UserSteppingAction
{
public:
  void UserSteppingAction(const G4Step *) override;
};
#endif
