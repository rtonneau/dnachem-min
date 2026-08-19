/// \file PhysicsList.hh
/// \brief Definition of the PhysicsList class

#ifndef PhysicsList_h
#define PhysicsList_h 1

#include "G4VModularPhysicsList.hh"
#include "globals.hh"

class G4VPhysicsConstructor;

class PhysicsList : public G4VModularPhysicsList
{
public:
  explicit PhysicsList();
  ~PhysicsList() override;

  // void ConstructParticle() override;
  // void ConstructProcess() override;

  void SetDNAPhysics(const G4String &name);
  void SetDNAChemistry(const G4String &name);

  inline G4bool IsChemistryEnabled() const
  {
    return !fChemDNAName.empty();
  }

private:
  G4String fChemDNAName;
  G4String fPhysDNAName;
  // std::unique_ptr<G4VPhysicsConstructor> fEmDNAChemistryList = nullptr;
  // std::unique_ptr<G4VPhysicsConstructor> fEmDNAPhysicsList = nullptr;
};
#endif
