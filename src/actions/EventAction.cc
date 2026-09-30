// EventAction.cc
#include "actions/EventAction.hh"
#include "core/DnaLogger.hh"
#include "core/OutputDir.hh"
#include "scoring/PreChemicalFiles.hh"

#include "G4DNAChemistryManager.hh"
#include "G4PhysChemIO.hh"
#include "G4RunManager.hh"
#include "G4Run.hh"

#include <memory>

EventAction::EventAction() = default;
EventAction::~EventAction() = default;

void EventAction::BeginOfEventAction(const G4Event *event)
{
    const G4int eventId = event->GetEventID();
    const G4String eventPrefix = G4String("[EventAction] Event ") + std::to_string(eventId);

    DnaLogger::Print(DnaLogger::Level::Info,
                     eventPrefix + ": Begin of Event");
    if (G4DNAChemistryManager::GetInstanceIfExists() != nullptr)
    {
        // One pre-chemical file per event, staged until the next dump (ADR 0005
        // addendum). A fresh writer per event: FormattedText::CloseFile() is a
        // no-op until the first record, so a reused writer could keep a stale
        // stream open.
        const G4String dir = OutputDir::GetDirectory();
        std::string err;
        if (!PreChemicalFiles::EnsureStagingDir(dir, err))
        {
            G4Exception("EventAction::BeginOfEventAction", "PreChemicalStagingDir",
                        FatalException, err.c_str());
        }
        const G4int runId = G4RunManager::GetRunManager()->GetCurrentRun()->GetRunID();
        const std::string fileName = PreChemicalFiles::StagingDir(dir) + "/" +
                                     PreChemicalFiles::StagedFileName(runId, eventId);

        auto *chemistryManager = G4DNAChemistryManager::Instance();
        chemistryManager->SetPhysChemIO(std::make_unique<G4PhysChemIO::FormattedText>());
        chemistryManager->WriteInto(fileName);
        DnaLogger::Print(DnaLogger::Level::Debug,
                         eventPrefix + ": pre-chemical file " + fileName);

        chemistryManager->BeginOfEventAction(event);
    }
}

void EventAction::EndOfEventAction(const G4Event *event)
{
    const G4int eventId = event->GetEventID();
    const G4String eventPrefix = G4String("[EventAction] Event ") + std::to_string(eventId);
    DnaLogger::Print(DnaLogger::Level::Info,
                     eventPrefix + ": End of Event");

    if (G4DNAChemistryManager::GetInstanceIfExists() != nullptr)
    {
        G4DNAChemistryManager::Instance()->EndOfEventAction(event);
        // Destroying the writer closes the event's file, even when the
        // chemistry stage never ran for this event.
        G4DNAChemistryManager::Instance()->SetPhysChemIO(nullptr);
    }
}
