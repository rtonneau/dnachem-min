/// \file RunAction.cc
/// \brief Implementation of the RunAction class

#include "RunAction.hh"

#include "DetectorConstruction.hh"
#include "PrimaryGeneratorAction.hh"
#include "Run.hh"

#include "G4DNAChemistryManager.hh"

#include "G4AccumulableManager.hh"
#include "G4Run.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4UnitsTable.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

RunAction::RunAction() : G4UserRunAction()
{
}
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

RunAction::~RunAction() {}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4Run *RunAction::GenerateRun()
{
    Run *run = new Run();
    return run;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::BeginOfRunAction(const G4Run *run)
{
    // ensure that the chemistry is notified!
    if (G4DNAChemistryManager::GetInstanceIfExists() != nullptr)
        G4DNAChemistryManager::GetInstanceIfExists()->BeginOfRunAction(run);

    G4cout << "### Run " << run->GetRunID() << " starts." << G4endl;

    // informs the runManager to save random number seed
    G4RunManager::GetRunManager()->SetRandomNumberStore(false);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::EndOfRunAction(const G4Run *run)
{
    // ensure that the chemistry is notified!
    if (G4DNAChemistryManager::GetInstanceIfExists() != nullptr)
        G4DNAChemistryManager::GetInstanceIfExists()->EndOfRunAction(run);

    G4int nofEvents = run->GetNumberOfEvent();
    if (nofEvents == 0)
        return;

    if (IsMaster())
    {
    }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
