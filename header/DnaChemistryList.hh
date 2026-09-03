/// \file DnaChemistryList.hh
/// \brief Definition of the DnaChemistryList class
///
/// Project chemical stage, replacing the macro-driven `/chem/species` +
/// `/chem/reaction/add` blocks. Inherits BOTH `G4VUserChemistryList`
/// (molecules / reactions / time-step model) and `G4VPhysicsConstructor`
/// (driven directly by PhysicsList, UHDR-style).
///
/// Base chemistry: pure-water radiolysis (7 tracked species, 9 reactions).
/// Optional add-on: dissolved O2 as a bulk scavenger — enabled with
/// `/chem/env/O2 <percent>`; when on, the O2 sub-system of the Geant4-DNA
/// `UHDR` example is added (bulk-O2 scavenging + O2-/HO2/HO2- chemistry +
/// per-molecule `G4DNAScavengerProcess`).
///
/// Time-step model: SBS default; IRT rejected (fatal); IRT_syn allowed.

#ifndef DnaChemistryList_h
#define DnaChemistryList_h 1

#include "G4VPhysicsConstructor.hh"
#include "G4VUserChemistryList.hh"
#include "globals.hh"

class G4DNABoundingBox;
class G4DNAMolecularReactionTable;
class DnaChemistryWorld;

class DnaChemistryList : public G4VUserChemistryList, public G4VPhysicsConstructor
{
public:
  DnaChemistryList();
  ~DnaChemistryList() override = default;

  // --- G4VPhysicsConstructor ---
  void ConstructParticle() override { ConstructMolecule(); }
  void ConstructProcess() override;

  // --- G4VUserChemistryList ---
  void ConstructMolecule() override;
  void ConstructDissociationChannels() override;
  void ConstructReactionTable(G4DNAMolecularReactionTable* reactionTable) override;
  void ConstructTimeStepModel(G4DNAMolecularReactionTable* reactionTable) override;

private:
  void GuardTimeStepModel(const G4String& caller) const;

  /// The project chemistry world, via the run manager's detector.
  const DnaChemistryWorld* ChemistryWorld(const G4String& caller) const;

  /// O2 / O2- / HO2 / HO2- / O- / O3- reactions between diffusing species
  /// (UHDR: ChemOxygenWaterBuilder). Added only when O2 is enabled.
  void ConstructOxygenReactionTable(G4DNAMolecularReactionTable* reactionTable) const;

  /// Per-molecule G4DNAScavengerProcess for reactions with the bulk species
  /// O2(B) / H3O+(B) / OH-(B) / H2O (UHDR: EmDNAChemistry::ConstructProcess).
  void RegisterOxygenScavengerProcesses(const G4DNABoundingBox& boundary) const;
};

#endif // DnaChemistryList_h
