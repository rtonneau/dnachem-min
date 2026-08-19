/// \file StackingAction.cc
/// \brief Implementation of the StackingAction class

#include "StackingAction.hh"
#include "PhysicsList.hh"
#include "G4RunManager.hh"

#include "G4SystemOfUnits.hh"
#include "G4ITTrackHolder.hh"
#include "G4AnalysisManager.hh"
#include "G4RunManager.hh"
#include "G4DNAChemistryManager.hh"
#include "G4StackManager.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

StackingAction::StackingAction() : G4UserStackingAction()
{
  this->physList = dynamic_cast<const PhysicsList *>(G4RunManager::GetRunManager()->GetUserPhysicsList());
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void StackingAction::NewStage()
{
  if (this->physList->IsChemistryEnabled() && this->stackManager->GetNTotalTrack() == 0)
  // if (this->stackManager->GetNTotalTrack() == 0)
  {
    G4cout << "Physics stage ends" << G4endl;
    // --- NEW CODE START ---

    // 1. Get current Event ID
    const G4Event *currentEvent = G4RunManager::GetRunManager()->GetCurrentEvent();
    G4cout << "[Stacking] G4Event pointer: " << currentEvent << G4endl;
    // --- NEW CODE END ---
    G4DNAChemistryManager::Instance()->Run(); // starts chemistry
    G4cout << "[Stacking] Chemistry started" << G4endl;
  }
}
