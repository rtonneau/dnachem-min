#include "RunAccumulatorMessenger.hh"
#include "RunAccumulator.hh"
#include "OutputDir.hh"
#include "ScoreSpecies.hh"

#include "G4UIcmdWithAString.hh"
#include "G4SDManager.hh"
#include "G4MultiFunctionalDetector.hh"
#include "G4AnalysisManager.hh"
#include "G4UnitsTable.hh"

#include <fstream>

RunAccumulatorMessenger::RunAccumulatorMessenger()
    : G4UImessenger()
{
    fpDumpCmd = new G4UIcmdWithAString("/run/dumpDataAndReset", this);
    fpDumpCmd->SetGuidance(
        "Write Species/Reactions/EnergyDeposit/PhysicsInteractions output "
        "files for everything accumulated since the last dump (or program "
        "start), then reset all counters. Optional prefix is prepended "
        "literally to every output filename (no separator inserted). Fatal "
        "if prefix was already used earlier in this run.");
    fpDumpCmd->SetParameterName("prefix", /*omittable=*/true);
    fpDumpCmd->SetDefaultValue("");
    fpDumpCmd->AvailableForStates(G4State_Idle);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

RunAccumulatorMessenger::~RunAccumulatorMessenger()
{
    delete fpDumpCmd;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

void RunAccumulatorMessenger::SetNewValue(G4UIcommand *command, G4String newValue)
{
    if (command == fpDumpCmd)
    {
        G4String err;
        if (!DumpAndReset(newValue, /*enforceUniqueness=*/true, err))
        {
            G4Exception("RunAccumulatorMessenger::SetNewValue", "DuplicateDumpPrefix",
                        FatalException, err.c_str());
        }
    }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

G4String RunAccumulatorMessenger::GetCurrentValue(G4UIcommand * /*command*/)
{
    return "";
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

void RunAccumulatorMessenger::FlushIfPending(const G4String &autoPrefix)
{
    if (!RunAccumulator::HasPendingData())
        return;

    G4String err;
    DumpAndReset(autoPrefix, /*enforceUniqueness=*/false, err);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

G4bool RunAccumulatorMessenger::DumpAndReset(const G4String &prefix, G4bool enforceUniqueness,
                                              G4String &err)
{
    if (!RunAccumulator::TryReservePrefix(prefix, enforceUniqueness, err))
        return false;

    OutputDir::SetPrefix(prefix);

    // Species: the ScoreSpecies scorer is itself the persistent,
    // SD-registered accumulator (unlike energy/reactions/interactions,
    // which RunAccumulator tracks separately) -- same lookup Run::Run()
    // uses.
    auto *mfdet = dynamic_cast<G4MultiFunctionalDetector *>(
        G4SDManager::GetSDMpointer()->FindSensitiveDetector("mfDetector"));
    if (mfdet != nullptr)
    {
        G4int collectionId = G4SDManager::GetSDMpointer()->GetCollectionID("mfDetector/Species");
        auto *scorer = dynamic_cast<ScoreSpecies *>(mfdet->GetPrimitive(collectionId));
        if (scorer != nullptr)
        {
            scorer->ASCII();          // Species.Txt (human-readable)
            scorer->OutputAndClear(); // Species_nt_species(_all).csv, then clears the scorer
        }
    }

    // Reactions
    const ReactionCounter &reactionCounter = RunAccumulator::GetAccumulatedReactionCounter();
    std::ofstream reactionsOut(OutputDir::Resolve("Reactions.Txt"));
    reactionCounter.WriteAscii(reactionsOut);
    reactionsOut.close();

    G4AnalysisManager *analysisManager = G4AnalysisManager::Instance();
    analysisManager->SetDefaultFileType("csv");
    reactionCounter.WriteCsv(analysisManager);

    std::ofstream metadataOut(OutputDir::Resolve("ReactionsMetadata.csv"));
    reactionCounter.WriteMetadata(metadataOut);
    metadataOut.close();

    // G4AnalysisManager's per-ntuple file registry (keyed by resolved
    // filename) isn't cleared by CloseFile() alone -- a later dump cycle
    // reusing the same ntuple names ("species"/"reactions") under a new
    // prefix would otherwise report "already in use" and silently drop or
    // rename its CSV output. Clear() (not Reset(), which only resets the
    // ntuple/histogram bookings, not the file-name registry) empties that
    // registry so each dump cycle starts clean. Harmless here -- this
    // project never uses G4AnalysisManager's histogram features that
    // Clear() also resets.
    analysisManager->Clear();

    // Energy
    std::ofstream energyOut(OutputDir::Resolve("EnergyDeposit.Txt"));
    energyOut << "Total energy deposited in simulation volume: "
              << G4BestUnit(RunAccumulator::GetAccumulatedEnergy(), "Energy") << "\n";
    energyOut.close();

    // Physics interactions
    const PhysicsInteractionCounter &interactionCounter =
        RunAccumulator::GetAccumulatedInteractionCounter();
    std::ofstream interactionsOut(OutputDir::Resolve("PhysicsInteractions.Txt"));
    interactionCounter.WriteAscii(interactionsOut);
    interactionsOut.close();

    std::ofstream interactionsCsv(OutputDir::Resolve("PhysicsInteractions.csv"));
    interactionCounter.WriteCsv(interactionsCsv);
    interactionsCsv.close();

    RunAccumulator::ClearAccumulated();
    OutputDir::SetPrefix("");

    // Plain G4cout (not DnaLogger): this is the run.successMarkers line the
    // project's own smoke-test verification checks for -- see
    // .claude/.claude-project.json and .claude/geant4-instructions.md.
    G4cout << "[RunAccumulatorMessenger] dumped and reset (prefix='" << prefix << "')" << G4endl;

    return true;
}
