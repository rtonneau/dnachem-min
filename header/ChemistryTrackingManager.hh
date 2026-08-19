#ifndef ChemistryTrackingManager_hh
#define ChemistryTrackingManager_hh 1

#include "G4ITTrackingInteractivity.hh"

class ChemistryTrackingManager : public G4ITTrackingInteractivity
{
public:
  ChemistryTrackingManager() = default;
  ~ChemistryTrackingManager() override;

  void AppendStep(G4Track *, G4Step *) override;
  void Finalize() override;

  void SetUserAction(G4UserSteppingAction *);
  const G4UserSteppingAction *GetUserSteppingAction() const;

private:
  G4UserSteppingAction *fUserSteppingAction = nullptr;
};

inline void ChemistryTrackingManager::SetUserAction(G4UserSteppingAction *steppingAction)
{
  fUserSteppingAction = steppingAction;
}

inline const G4UserSteppingAction *ChemistryTrackingManager::GetUserSteppingAction() const
{
  return fUserSteppingAction;
}
#endif
