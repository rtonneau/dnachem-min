#include "ChemistrySteppingAction.hh"

#include "G4MoleculeCounterManager.hh"
#include "G4Step.hh"

void ChemistrySteppingAction::UserSteppingAction(const G4Step *aStep)
{
  if (G4MoleculeCounterManager::Instance()->GetIsActive())
    G4MoleculeCounterManager::Instance()->NotifyOfStep(aStep);
}
