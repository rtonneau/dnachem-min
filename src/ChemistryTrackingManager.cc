#include "ChemistryTrackingManager.hh"

#include "G4Event.hh"
#include "G4EventManager.hh"
#include "G4MoleculeCounterManager.hh"
#include "G4UserSteppingAction.hh"
#include "G4VSensitiveDetector.hh"

ChemistryTrackingManager::~ChemistryTrackingManager()
{
  // Ensure this manager's stepping action is not handled by the event manager
  auto eventManager = G4EventManager::GetEventManager();
  if (!(!eventManager || fUserSteppingAction == eventManager->GetUserSteppingAction()))
    delete this->fUserSteppingAction;
}

void ChemistryTrackingManager::AppendStep(G4Track * /*track*/, G4Step *step)
{
  if (step->GetPreStepPoint()->GetPhysicalVolume() != nullptr && step->GetControlFlag() != AvoidHitInvocation)
  {
    auto sensitiveDetector = step->GetPreStepPoint()->GetSensitiveDetector();
    if (sensitiveDetector != nullptr)
    {
      sensitiveDetector->Hit(step);
    }
  }

  if (this->fUserSteppingAction)
    this->fUserSteppingAction->UserSteppingAction(step);
}

void ChemistryTrackingManager::Finalize()
{
  if (G4MoleculeCounterManager::Instance()->GetIsActive())
    G4MoleculeCounterManager::Instance()->NotifyOfFinalize();
}
