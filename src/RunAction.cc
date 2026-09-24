/// \file RunAction.cc
/// \brief Implementation of the RunAction class

#include "RunAction.hh"

#include "DetectorConstruction.hh"
#include "DnaChemistryList.hh"
#include "DnaLogger.hh"
#include "PhysicsList.hh"
#include "PrimaryGeneratorAction.hh"
#include "Run.hh"
#include "RunAccumulator.hh"

#include "G4DNAChemistryManager.hh"

#include "G4AccumulableManager.hh"
#include "G4Run.hh"
#include "G4RunManager.hh"

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

    if (IsMaster())
    {
        G4cout << "### Run " << run->GetRunID() << " starts." << G4endl;

        // Resolve any /chem/reaction/timeBinsFixed or timeBinsList macro
        // command into ReactionCounter's shared bin-edge table. Must happen
        // here (not earlier) since "fixed" mode depends on the chemistry
        // scheduler's end time, which a macro may still override between
        // /run/initialize and /run/beamOn.
        auto *physicsList = dynamic_cast<const PhysicsList *>(
            G4RunManager::GetRunManager()->GetUserPhysicsList());
        if (physicsList != nullptr)
            physicsList->GetChemistryList()->ApplyReactionTimeBinning();
    }

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
        auto *masterRun = static_cast<const Run *>(run);

        // Species yields keep accumulating on their own (the ScoreSpecies
        // scorer is itself the persistent, SD-registered accumulator, fed by
        // Run::Merge -> AbsorbResultsFromWorkerScorer). Energy deposit and
        // the two counters don't have that luxury -- Run is recreated fresh
        // every beamOn -- so fold this run's totals into RunAccumulator's
        // persistent, cross-run storage instead. Nothing is written to disk
        // here: issue /run/dumpDataAndReset (or let the exit-time safety net
        // in sim.cc fire) to flush everything.
        RunAccumulator::Accumulate(masterRun->GetSumDose(), *masterRun->GetReactionCounter(),
                                    *masterRun->GetInteractionCounter());

        DnaLogger::Print(DnaLogger::Level::Info,
                          "[RunAction] accumulated this run's energy/reaction/interaction data -- "
                          "use /run/dumpDataAndReset to write everything to files");
    }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
