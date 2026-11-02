/// \file RunAction.cc
/// \brief Implementation of the RunAction class

#include "actions/RunAction.hh"

#include "geometry/DetectorConstruction.hh"
#include "chemistry/DnaChemistryList.hh"
#include "core/DnaLogger.hh"
#include "physics/PhysicsList.hh"
#include "actions/PrimaryGeneratorAction.hh"
#include "actions/Run.hh"
#include "scoring/RunAccumulator.hh"
#include "scoring/RunManifest.hh"
#include "scoring/ScoreSpecies.hh"
#include "scoring/SpeciesSampleTimes.hh"
#include "scoring/SpeciesSampleTimesMessenger.hh"

#include "G4DNAChemistryManager.hh"

#include "G4AccumulableManager.hh"
#include "G4MultiFunctionalDetector.hh"
#include "G4Run.hh"
#include "G4RunManager.hh"
#include "G4Scheduler.hh"
#include "G4SDManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4Threading.hh"

#include <sstream>
#include <vector>

namespace
{
    // Installs the /scoring/species/... sample-time grid on this thread's
    // ScoreSpecies. Built here, at run start, from this thread's chemistry
    // end time, because /scheduler/endTime may be issued after
    // /run/initialize.
    void ApplySpeciesSampleTimes()
    {
        std::vector<double> dropped;
        const std::vector<double> grid = SpeciesSampleTimes::BuildGrid(
            SpeciesSampleTimesMessenger::Current(), G4Scheduler::Instance()->GetEndTime(),
            &dropped);

        // Same lookup as RunAccumulatorMessenger::WriteAllAndReset.
        auto *mfdet = dynamic_cast<G4MultiFunctionalDetector *>(
            G4SDManager::GetSDMpointer()->FindSensitiveDetector("mfDetector"));
        if (mfdet == nullptr)
            return;
        G4int collectionId = G4SDManager::GetSDMpointer()->GetCollectionID("mfDetector/Species");
        auto *scorer = dynamic_cast<ScoreSpecies *>(mfdet->GetPrimitive(collectionId));
        if (scorer == nullptr)
            return;

        scorer->ClearTimeToRecord();
        for (double t : grid)
            scorer->AddTimeToRecord(t);
        SpeciesSampleTimesMessenger::SetLastGridSize(grid.size());

        // One copy of the warnings, not one per worker.
        if (!G4Threading::IsMultithreadedApplication() || G4Threading::G4GetThreadId() == 0)
        {
            for (double t : dropped)
            {
                std::ostringstream msg;
                msg << "[RunAction] /scoring/species/timesList: " << t / ns
                    << " ns is after the chemistry end time ("
                    << G4Scheduler::Instance()->GetEndTime() / ns << " ns), dropped";
                DnaLogger::Print(DnaLogger::Level::Warning, msg.str());
            }
        }
    }
} // namespace

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
    if (IsMaster())
        fRunStart = std::chrono::steady_clock::now();

    // ensure that the chemistry is notified!
    if (G4DNAChemistryManager::GetInstanceIfExists() != nullptr)
        G4DNAChemistryManager::GetInstanceIfExists()->BeginOfRunAction(run);

    // Give the reaction/interaction counters a per-run lifetime. On a thread
    // that owns the TimeStepAction/SteppingAction (Serial master, MT worker)
    // the new Run points at their live, persistent counters; on the MT master
    // it points at its own, already empty ones. This is the only reset.
    auto *thisRun = static_cast<const Run *>(run);
    thisRun->GetReactionCounter()->Clear();
    thisRun->GetInteractionCounter()->Clear();

    // Species sample times, on the threads that fill the scorer (Serial
    // master, MT workers); the MT master scorer only receives merged results.
    if (!IsMaster() || !G4Threading::IsMultithreadedApplication())
        ApplySpeciesSampleTimes();

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
        RunAccumulator::Accumulate(masterRun->GetSumDose(), nofEvents,
                                    *masterRun->GetReactionCounter(),
                                    *masterRun->GetInteractionCounter());

        // One entry per run for the dump's manifest (beam, events, seed,
        // this run's energy deposit, Begin->EndOfRunAction wall time).
        RunManifest::RecordRun(
            *masterRun,
            std::chrono::duration<double>(std::chrono::steady_clock::now() - fRunStart).count());

        DnaLogger::Print(DnaLogger::Level::Info,
                          "[RunAction] accumulated this run's energy/reaction/interaction data -- "
                          "use /run/dumpDataAndReset to write everything to files");
    }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
