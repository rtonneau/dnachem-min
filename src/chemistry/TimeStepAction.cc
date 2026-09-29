#include "chemistry/TimeStepAction.hh"

#include "core/DnaLogger.hh"
#include "chemistry/ReactionTableDump.hh"

#include "G4DNAMolecularReactionTable.hh"
#include "G4Scheduler.hh"
#include "G4Molecule.hh"
#include "G4MolecularConfiguration.hh"
#include "G4SystemOfUnits.hh"

#include <vector>

// #include "G4ITScheduler.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

TimeStepAction::TimeStepAction() : G4UserTimeStepAction()
{
  /**
   * Inform G4ITTimeStepper of the selected minimum time steps
   * eg : from 1 picosecond to 10 picosecond, the minimum time
   * step that the TimeStepper can returned is 0.1 picosecond.
   *
   * Case 1) If the rection model calculates a minimum reaction time
   * bigger than the user defined time step, the reaction model wins
   *
   * Case 2) If an interaction process with the continuous medium
   * calculates a time step less than the selected minimum time step,
   * the interaction process wins
   */

  AddTimeStep(1 * picosecond, 0.1 * picosecond);
  AddTimeStep(10 * picosecond, 1 * picosecond);
  AddTimeStep(100 * picosecond, 10 * picosecond);
  AddTimeStep(1000 * picosecond, 100 * picosecond);
  AddTimeStep(10000 * picosecond, 1000 * picosecond);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

TimeStepAction::~TimeStepAction() {}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

TimeStepAction::TimeStepAction(const TimeStepAction &other) : G4UserTimeStepAction(other) {}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
TimeStepAction &TimeStepAction::operator=(const TimeStepAction &rhs)
{
  if (this == &rhs)
    return *this; // handle self assignment
  // assignment operator
  return *this;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void TimeStepAction::StartProcessing()
{
  // You want to know why the simulation stopped ?
  // G4ITScheduler::Instance()->WhyDoYouStop();
  // At the end of the simulation, information will be printed
  // It is better to place this command before the simulation starts
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void TimeStepAction::UserPreTimeStepAction() {}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void TimeStepAction::UserPostTimeStepAction() {}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

// Counts each bimolecular reaction that fires, binned by time, so a
// per-reaction rate-over-time output can be written at end of run (see
// Run::Merge / RunAction::EndOfRunAction).
void TimeStepAction::UserReactionAction(const G4Track &a, const G4Track &b,
                                        const std::vector<G4Track *> * /*products*/)
{
  const auto *molA = GetMolecule(a)->GetMolecularConfiguration();
  const auto *molB = GetMolecule(b)->GetMolecularConfiguration();

  const auto *reactionData =
      G4DNAMolecularReactionTable::GetReactionTable()->GetReactionData(molA, molB);
  if (reactionData == nullptr) {
    DnaLogger::Print(DnaLogger::Level::Warning,
                     "[TimeStepAction] reaction fired with no matching reaction-table entry: "
                     + molA->GetName() + " + " + molB->GetName());
    return;
  }

  const G4String& label = ReactionTableDump::LabelFor(reactionData->GetReactionID());
  fReactionCounter.Record(label, G4Scheduler::Instance()->GetGlobalTime());

  // Example to display reactions with product
  // S. Incerti, H. Tran
  // 2019/01/24

  /*
  if (products)
  {
    G4cout << G4endl;
    G4int nbProducts = products->size();
    for (int i = 0 ; i < nbProducts ; i ++)
    {
      G4cout << "-> A = "
        << GetMolecule(&a)->GetName() << " (TrackID=" << a.GetTrackID() << ")"
        << " reacts with B = "
        << GetMolecule(&b)->GetName() << " (TrackID=" << b.GetTrackID() << ")"
        << " creating product " << i+1 << " ="
        << GetMolecule((*products)[i])->GetName()
        << G4endl ;

      G4cout
      <<" A position: x(nm)="<<a.GetPosition().getX()/nm
      <<" y(nm)="<<a.GetPosition().getY()/nm
      <<" z(nm)="<<a.GetPosition().getZ()/nm
      <<G4endl;

      G4cout
      <<" B position: x(nm)="<<b.GetPosition().getX()/nm
      <<" y(nm)="<<b.GetPosition().getY()/nm
      <<" z(nm)="<<b.GetPosition().getZ()/nm
      <<G4endl;

      G4cout
      <<" Product " << i+1 << "position: x(nm)="<<(*products)[i]->GetPosition().getX()/nm
      <<" y(nm)="<<a.GetPosition().getY()/nm
      <<" z(nm)="<<a.GetPosition().getZ()/nm
      <<G4endl;
    }
  }

  else

  {
     G4cout << G4endl;
     G4cout << "-> A = "
        << GetMolecule(&a)->GetName() << " (TrackID=" << a.GetTrackID() << ")"
        << " reacts with B = "
        << GetMolecule(&b)->GetName() << " (TrackID=" << b.GetTrackID() << ")"
        << G4endl ;

      G4cout
      <<" A position: x(nm)="<<a.GetPosition().getX()/nm
      <<" y(nm)="<<a.GetPosition().getY()/nm
      <<" z(nm)="<<a.GetPosition().getZ()/nm
      <<G4endl;

      G4cout
      <<" B position: x(nm)="<<b.GetPosition().getX()/nm
      <<" y(nm)="<<b.GetPosition().getY()/nm
      <<" z(nm)="<<b.GetPosition().getZ()/nm
      <<G4endl;

  }
  */
}

void TimeStepAction::EndProcessing()
{
  DnaLogger::Print(DnaLogger::Level::Debug,
                   "[TimeStepAction] Chemistry ends at global time " +
                       std::to_string(G4Scheduler::Instance()->GetGlobalTime() / picosecond) + " ps");
}
