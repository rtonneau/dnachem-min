#include "ChemistrySteppingAction.hh"

#include "G4MoleculeCounterManager.hh"
#include "G4Step.hh"

void ChemistrySteppingAction::UserSteppingAction(const G4Step *aStep)
{
  G4cout << "[ChemistrySteppingAction] UserSteppingAction" << G4endl;
  if (G4MoleculeCounterManager::Instance()->GetIsActive())
    G4MoleculeCounterManager::Instance()->NotifyOfStep(aStep);
}
