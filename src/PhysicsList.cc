/// \file PhysicsList.cc
/// \brief Implementation of the PhysicsList class

#include "PhysicsList.hh"

#include "G4EmDNAChemistry.hh"
#include "G4EmDNAChemistry_option1.hh"
#include "G4EmDNAChemistry_option2.hh"
#include "G4EmDNAChemistry_option3.hh"
#include "G4EmDNAPhysics.hh"

#include "G4EmDNAPhysics_option1.hh"
#include "G4EmDNAPhysics_option2.hh"
#include "G4EmDNAPhysics_option3.hh"
#include "G4EmDNAPhysics_option4.hh"
#include "G4EmDNAPhysics_option5.hh"
#include "G4EmDNAPhysics_option6.hh"
#include "G4EmDNAPhysics_option7.hh"
#include "G4EmDNAPhysics_option8.hh"
#include "G4EmParameters.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"

PhysicsList::PhysicsList()
    : G4VModularPhysicsList(),
      fPhysDNAName(""),
      fChemDNAName("")
{

  G4double currentDefaultCut = 1. * um;
  // fixe lower limit for cut
  G4ProductionCutsTable::GetProductionCutsTable()->SetEnergyRange(50 * eV, 1 * GeV);
  this->SetDefaultCutValue(currentDefaultCut);
  this->SetVerboseLevel(0);

  // this->SetDNAPhysics("G4EmDNAPhysics_option6");
  this->SetDNAPhysics("G4EmDNAPhysics");
  this->SetDNAChemistry("G4EmDNAChemistry_option3");
  // Suppress EM parameters verbose output
  G4EmParameters::Instance()->SetVerbose(0); // ← add this
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

PhysicsList::~PhysicsList() = default;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

/*
void PhysicsList::ConstructParticle()
{
  if (this->fEmDNAPhysicsList != nullptr)
  {
    this->fEmDNAPhysicsList->ConstructParticle();
  }
  if (fEmDNAChemistryList != nullptr)
  {
    fEmDNAChemistryList->ConstructParticle();
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void PhysicsList::ConstructProcess()
{
  AddTransportation();
  if (this->fEmDNAPhysicsList != nullptr)
  {
    this->fEmDNAPhysicsList->ConstructProcess();
  }
  // G4-DNA chemistry processes must be constructed after all other processes
  if (this->fEmDNAChemistryList != nullptr)
  {
    this->fEmDNAChemistryList->ConstructProcess();
  }
}
*/

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void PhysicsList::SetDNAChemistry(const G4String &name)
{
  if (name == this->fChemDNAName)
  {
    return;
  }

  if (this->fPhysDNAName.empty())
  {
    G4Exception("PhysicsList::SetDNAChemistry", "SetDNAChemistryBeforePhysics",
                FatalException, "Setting chemistry before physics - Aborting simulation");
  }

  G4VPhysicsConstructor *to_register = nullptr;
  if (name == "G4EmDNAChemistry")
  {
    // fEmDNAChemistryList = std::make_unique<G4EmDNAChemistry>();
    to_register = new G4EmDNAChemistry();
  }
  else if (name == "G4EmDNAChemistry_option1")
  {
    // fEmDNAChemistryList = std::make_unique<G4EmDNAChemistry_option1>();
    to_register = new G4EmDNAChemistry_option1();
  }
  else if (name == "G4EmDNAChemistry_option2")
  {
    // fEmDNAChemistryList = std::make_unique<G4EmDNAChemistry_option2>();
    to_register = new G4EmDNAChemistry_option2();
  }
  else if (name == "G4EmDNAChemistry_option3")
  {
    // fEmDNAChemistryList = std::make_unique<G4EmDNAChemistry_option3>();
    to_register = new G4EmDNAChemistry_option3();
  }
  else
  {
    G4Exception("PhysicsList::SetDNAChemistry", "InvalidG4EmDNAChemistryName",
                FatalException, "Aborting simulation - invalid G4EmDNAChemistry name");
    return;
  }

  if (to_register == nullptr) // Normally this should never happen, but just in case
  {
    G4Exception("PhysicsList::SetDNAChemistry", "NullConstructor",
                FatalException, "to_register is null - this should never happen");
    return;
  }
  G4cout << "===== Enabled chemistry ==== " << name << G4endl;
  to_register->SetVerboseLevel(verboseLevel);
  this->RegisterPhysics(to_register);
  this->fChemDNAName = name;
}

void PhysicsList::SetDNAPhysics(const G4String &name)
{

  if (name == fPhysDNAName)
  {
    return;
  }
  if (verboseLevel > 0)
  {
    G4cout << "===== Register constructor ==== " << name << G4endl;
  }

  G4VPhysicsConstructor *to_register = nullptr;
  if (name == "G4EmDNAPhysics")
  {
    // this->fEmDNAPhysicsList = std::make_unique<G4EmDNAPhysics>(verboseLevel);
    to_register = new G4EmDNAPhysics(verboseLevel);
  }
  else if (name == "G4EmDNAPhysics_option1")
  {
    // this->fEmDNAPhysicsList = std::make_unique<G4EmDNAPhysics_option1>(verboseLevel);
    to_register = new G4EmDNAPhysics_option1(verboseLevel);
  }
  else if (name == "G4EmDNAPhysics_option2")
  {
    // this->fEmDNAPhysicsList = std::make_unique<G4EmDNAPhysics_option2>(verboseLevel);
    to_register = new G4EmDNAPhysics_option2(verboseLevel);
  }
  else if (name == "G4EmDNAPhysics_option3")
  {
    // this->fEmDNAPhysicsList = std::make_unique<G4EmDNAPhysics_option3>(verboseLevel);
    to_register = new G4EmDNAPhysics_option3(verboseLevel);
  }
  else if (name == "G4EmDNAPhysics_option4")
  {
    // this->fEmDNAPhysicsList = std::make_unique<G4EmDNAPhysics_option4>(verboseLevel);
    to_register = new G4EmDNAPhysics_option4(verboseLevel);
  }
  else if (name == "G4EmDNAPhysics_option5")
  {
    // this->fEmDNAPhysicsList = std::make_unique<G4EmDNAPhysics_option5>(verboseLevel);
    to_register = new G4EmDNAPhysics_option5(verboseLevel);
  }
  else if (name == "G4EmDNAPhysics_option6")
  {
    // this->fEmDNAPhysicsList = std::make_unique<G4EmDNAPhysics_option6>(verboseLevel);
    to_register = new G4EmDNAPhysics_option6(verboseLevel);
  }
  else if (name == "G4EmDNAPhysics_option7")
  {
    // this->fEmDNAPhysicsList = std::make_unique<G4EmDNAPhysics_option7>(verboseLevel);
    to_register = new G4EmDNAPhysics_option7(verboseLevel);
  }
  else if (name == "G4EmDNAPhysics_option8")
  {
    // this->fEmDNAPhysicsList = std::make_unique<G4EmDNAPhysics_option8>(verboseLevel);
    to_register = new G4EmDNAPhysics_option8(verboseLevel);
  }
  else
  {
    G4Exception("PhysicsList::SetDNAPhysics", "InvalidG4EmDNAPhysicsName",
                FatalException, "Aborting simulation - invalid G4EmDNAPhysics name");
    return;
  }

  if (to_register == nullptr) // Normally this should never happen, but just in case
  {
    G4Exception("PhysicsList::SetDNAPhysics", "NullConstructor",
                FatalException, "to_register is null - this should never happen");
    return;
  }

  to_register->SetVerboseLevel(verboseLevel);
  this->RegisterPhysics(to_register);
  this->fPhysDNAName = name;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
