#ifndef CHEM4_Run_h
#define CHEM4_Run_h 1

#include "DetectorConstruction.hh"
#include "ReactionCounter.hh"
#include "PhysicsInteractionCounter.hh"

#include "G4Run.hh"
#include "globals.hh"

/// Run class
///
/// In RecordEvent() there is collected information event per event
/// from Hits Collections, and accumulated statistic for the run
class G4VPrimitiveScorer;
class DetectorConstruction;
class Run : public G4Run
{
public:
    Run();
    virtual ~Run();

    virtual void RecordEvent(const G4Event *);
    virtual void Merge(const G4Run *);

    G4double GetSumDose() const { return fSumEne; }
    G4VPrimitiveScorer *GetPrimitiveScorer() const { return fScorerRun; }
    ReactionCounter *GetReactionCounter() const { return fReactionCounter; }
    PhysicsInteractionCounter *GetInteractionCounter() const { return fInteractionCounter; }

private:
    G4double fSumEne;
    G4VPrimitiveScorer *fScorerRun;
    // Points at TimeStepAction's live counter on a worker thread (where
    // reactions are actually counted); on the master thread (which never
    // registers a TimeStepAction/runs chemistry) falls back to
    // fOwnedReactionCounter, used purely as the Merge() accumulation target.
    ReactionCounter *fReactionCounter;
    ReactionCounter fOwnedReactionCounter;
    // Same pattern as fReactionCounter/fOwnedReactionCounter, but for
    // SteppingAction's live PhysicsInteractionCounter.
    PhysicsInteractionCounter *fInteractionCounter;
    PhysicsInteractionCounter fOwnedInteractionCounter;
};

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#endif
