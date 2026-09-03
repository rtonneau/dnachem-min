/// \file DnaChemistryList.cc
/// \brief Implementation of the DnaChemistryList class
///
/// Structure mirrors G4EmDNAChemistry_option3 and the UHDR example's
/// EmDNAChemistry. Molecules + water dissociation channels come from
/// G4ChemDissociationChannels_option1. The base reaction set is the
/// pure-water radiolysis list that used to live in macro/beam.in.
///
/// When /chem/env/O2 selects a non-zero dissolved-O2 fraction, the O2
/// sub-system of the UHDR example is added:
///   - bulk-O2 scavenging + O2- / HO2 / HO2- / O- / O3- chemistry in the
///     reaction table  (UHDR: ChemOxygenWaterBuilder)
///   - per-molecule G4DNAScavengerProcess for reactions with the bulk
///     species O2(B) / H3O+(B) / OH-(B) / H2O
///     (UHDR: EmDNAChemistry::ConstructProcess)

#include "DnaChemistryList.hh"

#include "ChemUtils.hh"
#include "DetectorConstruction.hh"
#include "DnaChemistryWorld.hh"
#include "DnaLogger.hh"

#include "G4ChemDissociationChannels_option1.hh"
#include "G4ChemTimeStepModel.hh"
#include "G4DNABoundingBox.hh"
#include "G4DNAChemistryManager.hh"
#include "G4DNAMolecularReactionTable.hh"
#include "G4EmParameters.hh"
#include "G4MolecularConfiguration.hh"
#include "G4MoleculeTable.hh"
#include "G4PhysicsListHelper.hh"
#include "G4ProcessManager.hh"
#include "G4ProcessTable.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"

// Chemical-stage processes
#include "G4DNABrownianTransportation.hh"
#include "G4DNAElectronHoleRecombination.hh"
#include "G4DNAElectronSolvation.hh"
#include "G4DNAMolecularDissociation.hh"
#include "G4DNASancheExcitationModel.hh"
#include "G4DNAScavengerProcess.hh"
#include "G4DNAVibExcitation.hh"
#include "G4DNAWaterDissociationDisplacer.hh"

// Time-step models
#include "G4DNAIndependentReactionTimeModel.hh"
#include "G4DNAMolecularStepByStepModel.hh"
#include "G4DNAScavengerMaterial.hh"
#include "G4Scheduler.hh"

// Particles
#include "G4Electron.hh"
#include "G4Electron_aq.hh"
#include "G4H2O.hh"
#include "G4HO2.hh"
#include "G4Hydrogen.hh"
#include "G4O2.hh"
#include "G4O3.hh"
#include "G4Oxygen.hh"

#include "G4PhysicsConstructorFactory.hh"

#include <memory>
#include <vector>

G4_DECLARE_PHYSCONSTR_FACTORY(DnaChemistryList);

namespace
{
// Configuration tags containing the UTF-8 degree sign (0xC2 0xB0), spelled
// from explicit bytes so they match the names stored by
// G4ChemDissociationChannels_option1 whatever this file's source encoding is.
const G4String kOH = G4String("\xC2\xB0") + "OH";   // hydroxyl radical
const G4String kHO2 = G4String("HO2") + "\xC2\xB0"; // hydroperoxyl radical

using MolConf = const G4MolecularConfiguration*;

MolConf Conf(const G4String& name, const G4String& caller)
{
  auto* p = G4MoleculeTable::Instance()->GetConfiguration(name);
  if (p == nullptr) {
    G4Exception(("DnaChemistryList::" + caller).c_str(), "MissingSpecies", FatalException,
                (G4String("Unknown species configuration: ") + name).c_str());
  }
  return p;
}
}  // namespace

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DnaChemistryList::DnaChemistryList()
  : G4VUserChemistryList(true)
{
  // Register with the chemistry manager (also activates the chemical stage).
  // The (true) flag marks this object as a G4VPhysicsConstructor so the manager
  // releases - rather than deletes - it, leaving ownership with the PhysicsList
  // that holds it (avoids a double free).
  G4DNAChemistryManager::Instance()->SetChemistryList(this);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::GuardTimeStepModel(const G4String& caller) const
{
  const auto model = G4EmParameters::Instance()->GetTimeStepModel();
  if (model == G4ChemTimeStepModel::IRT || model == G4ChemTimeStepModel::Unknown) {
    G4ExceptionDescription ed;
    ed << "Chemistry time-step model '" << ChemUtils::GetCurrentTimeStepModelName()
       << "' is not supported by DnaChemistryList.\n"
       << "Use SBS (project default) or IRT_syn:  /process/chem/TimeStepModel SBS";
    G4Exception((G4String("DnaChemistryList::") + caller).c_str(), "BadTimeStepModel",
                FatalException, ed);
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

const DnaChemistryWorld* DnaChemistryList::ChemistryWorld(const G4String& caller) const
{
  const auto* detector = dynamic_cast<const DetectorConstruction*>(
    G4RunManager::GetRunManager()->GetUserDetectorConstruction());
  const auto* world =
    (detector != nullptr) ? dynamic_cast<const DnaChemistryWorld*>(detector->GetChemistryWorld())
                          : nullptr;
  if (world == nullptr) {
    G4Exception((G4String("DnaChemistryList::") + caller).c_str(), "NoChemistryWorld",
                FatalException, "Expected a DetectorConstruction owning a DnaChemistryWorld.");
  }
  return world;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::ConstructMolecule()
{
  // Standard radiolysis set + "(B)" bulk pseudo-species + water dissociation.
  G4ChemDissociationChannels_option1::ConstructMolecule();

  // Required by G4DNAScavengerProcess (member init: GetConfiguration("H2O")).
  G4MoleculeTable::Instance()->CreateConfiguration("H2O", G4H2O::Definition());

  // Historical /chem/species diffusion/radius overrides (chem6/IRT values).
  struct SpeciesTweak
  {
    G4String name;
    G4double diffusion;  // m^2/s
    G4double radius;     // nm
  };
  const std::vector<SpeciesTweak> tweaks = {
    {"O2", 2.4e-9, 0.17}, {"H3Op", 9.0e-9, 0.25}, {kOH, 2.8e-9, 0.22},
    {"e_aq", 4.9e-9, 0.50}, {"OHm", 5.0e-9, 0.33}, {"H", 7.0e-9, 0.19},
    {"H2", 5.0e-9, 0.14},
  };
  for (const auto& t : tweaks) {
    auto* conf = G4MoleculeTable::Instance()->GetConfiguration(t.name);
    if (conf == nullptr) {
      G4Exception("DnaChemistryList::ConstructMolecule", "MissingSpecies", FatalException,
                  (G4String("Unknown species configuration: ") + t.name).c_str());
      continue;
    }
    conf->SetDiffusionCoefficient(t.diffusion * (m2 / s));
    conf->SetVanDerVaalsRadius(t.radius * nm);
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::ConstructDissociationChannels()
{
  G4ChemDissociationChannels_option1::ConstructDissociationChannels();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::ConstructReactionTable(G4DNAMolecularReactionTable* reactionTable)
{
  GuardTimeStepModel("ConstructReactionTable");

  // Populate the chemistry-world bulk composition here (master-only phase ->
  // race-free), before it is read by G4DNAScavengerMaterial.
  auto* chemWorld =
    const_cast<DnaChemistryWorld*>(ChemistryWorld("ConstructReactionTable"));
  chemWorld->ConstructChemistryComponents();

  auto add = [reactionTable](MolConf a, MolConf b, G4double k,
                             std::initializer_list<MolConf> products) {
    auto* rd = new G4DNAMolecularReactionData(k * (1e-3 * m3 / (mole * s)), a, b);
    for (auto* p : products) {
      rd->AddProduct(p);
    }
    reactionTable->SetReaction(rd);
  };

  // Pure-water radiolysis (was: macro/beam.in "/chem/reaction/add"). Totally
  // diffusion-controlled, reaction type default (0); H2O products implicit.
  auto* e_aq = Conf("e_aq", "ConstructReactionTable");
  auto* H = Conf("H", "ConstructReactionTable");
  auto* H2 = Conf("H2", "ConstructReactionTable");
  auto* OH = Conf(kOH, "ConstructReactionTable");
  auto* OHm = Conf("OHm", "ConstructReactionTable");
  auto* H3Op = Conf("H3Op", "ConstructReactionTable");
  auto* H2O2 = Conf("H2O2", "ConstructReactionTable");

  add(H, H, 1.2e10, {H2});
  add(e_aq, H, 2.65e10, {H2, OHm});
  add(e_aq, e_aq, 0.5e10, {H2, OHm, OHm});
  add(H3Op, OHm, 1.43e11, {});
  add(e_aq, OH, 2.95e10, {OHm});
  add(OH, OH, 0.44e10, {H2O2});
  add(e_aq, H2O2, 1.41e10, {OHm, OH});
  add(e_aq, H3Op, 2.11e10, {H});
  add(OH, H, 1.44e10, {});

  if (chemWorld->IsOxygenScavengerEnabled()) {
    ConstructOxygenReactionTable(reactionTable);
  }

  DnaLogger::Print(DnaLogger::Level::Info, "[DnaChemistryList] reaction table constructed" +
                     G4String(chemWorld->IsOxygenScavengerEnabled() ? " (+ O2 scavenger)" : ""));
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::ConstructOxygenReactionTable(
  G4DNAMolecularReactionTable* reactionTable) const
{
  const auto model = G4EmParameters::Instance()->GetTimeStepModel();

  auto* e_aq = Conf("e_aq", "ConstructOxygenReactionTable");
  auto* H = Conf("H", "ConstructOxygenReactionTable");
  auto* OH = Conf(kOH, "ConstructOxygenReactionTable");
  auto* OHm = Conf("OHm", "ConstructOxygenReactionTable");
  auto* H2O2 = Conf("H2O2", "ConstructOxygenReactionTable");
  auto* HO2 = Conf(kHO2, "ConstructOxygenReactionTable");
  auto* HO2m = Conf("HO2m", "ConstructOxygenReactionTable");
  auto* Om = Conf("Om", "ConstructOxygenReactionTable");
  auto* O2 = Conf("O2", "ConstructOxygenReactionTable");
  auto* O2m = Conf("O2m", "ConstructOxygenReactionTable");
  auto* O3m = Conf("O3m", "ConstructOxygenReactionTable");

  // bulk-O2 scavenging - also registered per-molecule as G4DNAScavengerProcess
  // (SBS); kept here for the IRT_syn path, matching UHDR OxygenScavengerReaction.
  auto bulk = [reactionTable](MolConf a, MolConf b, G4double k, std::initializer_list<MolConf> pr) {
    auto* rd = new G4DNAMolecularReactionData(k * (1e-3 * m3 / (mole * s)), a, b);
    for (auto* p : pr) rd->AddProduct(p);
    reactionTable->SetReaction(rd);
  };
  bulk(e_aq, O2, 1.74e10, {O2m});
  bulk(H, O2, 2.1e10, {HO2});
  bulk(Om, O2, 3.7e9, {O3m});

  // O2- / HO2 / HO2- / O- / O3- chemistry between diffusing species
  // (UHDR ChemOxygenWaterBuilder::SecondOrderReactionExtended, O2 subset).
  auto add = [reactionTable, model](MolConf a, MolConf b, G4double k,
                                    std::initializer_list<MolConf> pr) {
    auto* rd = new G4DNAMolecularReactionData(k * (1e-3 * m3 / (mole * s)), a, b);
    for (auto* p : pr) rd->AddProduct(p);
    if (model != G4ChemTimeStepModel::SBS) rd->SetReactionType(1);
    reactionTable->SetReaction(rd);
  };
  add(OH, O2m, 1.07e10, {O2, OHm});
  add(e_aq, O2m, 1.3e10, {H2O2, OHm, OHm});
  add(H, O2m, 1.0e10, {HO2m});
  add(OH, HO2, 7.90e9, {O2});
  add(e_aq, HO2, 1.29e10, {HO2m});
  add(H, HO2, 1.0e10, {H2O2});
  add(HO2, HO2, 9.80e5, {H2O2, O2});
  add(HO2, O2m, 9.70e7, {HO2m, O2});
  add(e_aq, HO2m, 3.51e9, {Om, OHm});
  add(H, Om, 2.00e10, {OHm});
  add(e_aq, Om, 2.31e10, {OHm, OHm});
  add(OH, Om, 1.00e9, {HO2m});
  add(OH, O3m, 8.50e9, {O2m, HO2});
  add(O2m, Om, 6.00e8, {O2, OHm, OHm});
  add(HO2m, Om, 3.50e8, {O2m, OHm});
  add(Om, Om, 1.00e8, {H2O2, OHm, OHm});
  add(Om, O3m, 7.00e8, {O2m, O2m});
  add(O2m, O2m, 1.0e2, {H2O2, O2, OHm, OHm});
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::ConstructTimeStepModel(G4DNAMolecularReactionTable* /*reactionTable*/)
{
  GuardTimeStepModel("ConstructTimeStepModel");

  const auto model = G4EmParameters::Instance()->GetTimeStepModel();
  if (model == G4ChemTimeStepModel::SBS) {
    RegisterTimeStepModel(new G4DNAMolecularStepByStepModel(), 0);
  }
  else {  // IRT_syn (IRT / Unknown already rejected by the guard)
    RegisterTimeStepModel(new G4DNAIndependentReactionTimeModel(), 0);
  }

  DnaLogger::Print(DnaLogger::Level::Info,
                   G4String("[DnaChemistryList] time-step model = ") +
                     ChemUtils::GetCurrentTimeStepModelName());
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::ConstructProcess()
{
  GuardTimeStepModel("ConstructProcess");

  auto* ph = G4PhysicsListHelper::GetPhysicsListHelper();

  // Extend the Sanche vibrational-excitation model down to thermal energies.
  auto* vibProcess =
    G4ProcessTable::GetProcessTable()->FindProcess("e-_G4DNAVibExcitation", "e-");
  if (vibProcess != nullptr) {
    auto* vib = dynamic_cast<G4DNAVibExcitation*>(vibProcess);
    if (vib != nullptr) {
      auto* sanche = dynamic_cast<G4DNASancheExcitationModel*>(vib->EmModel());
      if (sanche != nullptr) {
        sanche->ExtendLowEnergyLimit(0.025 * eV);
      }
    }
  }

  // Electron solvation: free e- -> e_aq (register once if not already present).
  if (G4ProcessTable::GetProcessTable()->FindProcess("e-_G4DNAElectronSolvation", "e-") ==
      nullptr) {
    ph->RegisterProcess(new G4DNAElectronSolvation("e-_G4DNAElectronSolvation"),
                        G4Electron::Definition());
  }

  const auto model = G4EmParameters::Instance()->GetTimeStepModel();

  auto* moleculeTable = G4MoleculeTable::Instance();
  auto iterator = moleculeTable->GetDefintionIterator();
  iterator.reset();
  while (iterator()) {
    auto* moleculeDef = iterator.value();

    if (moleculeDef != G4H2O::Definition()) {
      // IRT is rejected by the guard; check kept for parity with option3.
      if (model != G4ChemTimeStepModel::IRT) {
        ph->RegisterProcess(new G4DNABrownianTransportation(), moleculeDef);
      }
    }
    else {
      moleculeDef->GetProcessManager()->AddRestProcess(new G4DNAElectronHoleRecombination(), 2);
      auto* dissociation = new G4DNAMolecularDissociation("H2O_DNAMolecularDecay");
      dissociation->SetDisplacer(moleculeDef, new G4DNAWaterDissociationDisplacer);
      moleculeDef->GetProcessManager()->AddRestProcess(dissociation, 1);
    }
  }

  auto* chemWorld = const_cast<DnaChemistryWorld*>(ChemistryWorld("ConstructProcess"));
  if (chemWorld->IsOxygenScavengerEnabled()) {
    RegisterOxygenScavengerProcesses(*chemWorld->GetChemistryBoundary());
  }

  // Triggers InitializeMaster() -> ConstructReactionTable() ->
  // DnaChemistryWorld::ConstructChemistryComponents(): the bulk composition is
  // populated after this call returns.
  G4DNAChemistryManager::Instance()->Initialize();

  // Install the bulk-scavenger material now that the composition is known, and
  // before G4DNAScavengerProcess::BuildPhysicsTable() queries the scheduler.
  // Runs once (serial) or per worker thread (MT); the scheduler is thread-local.
  if (chemWorld->IsOxygenScavengerEnabled()
      && G4Scheduler::Instance()->GetScavengerMaterial() == nullptr) {
    auto scavenger = std::make_unique<G4DNAScavengerMaterial>(chemWorld);
    scavenger->SetCounterAgainstTime();
    G4Scheduler::Instance()->SetScavengerMaterial(std::move(scavenger));
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::RegisterOxygenScavengerProcesses(const G4DNABoundingBox& boundary) const
{
  auto* ph = G4PhysicsListHelper::GetPhysicsListHelper();

  auto* e_aq = Conf("e_aq", "RegisterOxygenScavengerProcesses");
  auto* H = Conf("H", "RegisterOxygenScavengerProcesses");
  auto* OH = Conf(kOH, "RegisterOxygenScavengerProcesses");
  auto* Om = Conf("Om", "RegisterOxygenScavengerProcesses");
  auto* O2 = Conf("O2", "RegisterOxygenScavengerProcesses");
  auto* O2m = Conf("O2m", "RegisterOxygenScavengerProcesses");
  auto* O3m = Conf("O3m", "RegisterOxygenScavengerProcesses");
  auto* HO2 = Conf(kHO2, "RegisterOxygenScavengerProcesses");
  auto* HO2m = Conf("HO2m", "RegisterOxygenScavengerProcesses");
  auto* H2O2 = Conf("H2O2", "RegisterOxygenScavengerProcesses");
  auto* H3OpB = Conf("H3Op(B)", "RegisterOxygenScavengerProcesses");
  auto* OHmB = Conf("OHm(B)", "RegisterOxygenScavengerProcesses");
  auto* H2O = Conf("H2O", "RegisterOxygenScavengerProcesses");

  struct Rx
  {
    MolConf bulk;
    G4double rate;  // already dimensioned
    std::vector<MolConf> products;
    G4int type;
  };

  auto build = [&](G4MoleculeDefinition* def, MolConf mol, std::initializer_list<Rx> reactions) {
    auto* process = new G4DNAScavengerProcess("G4DNAScavengerProcess", boundary);
    for (const auto& r : reactions) {
      auto* rd = new G4DNAMolecularReactionData(r.rate, mol, r.bulk);
      for (auto* p : r.products) {
        rd->AddProduct(p);
      }
      if (r.type != 0) {
        rd->SetReactionType(r.type);
      }
      process->SetReaction(mol, rd);
    }
    ph->RegisterProcess(process, def);
  };

  const G4double M = 1e-3 * m3 / (mole * s);  // bimolecular unit (M^-1 s^-1)
  const G4double cW = 55.3;                   // bulk water molarity factor

  build(G4Hydrogen::Definition(), H, {{O2, 2.1e10 * M, {HO2}, 0}});

  build(G4Electron_aq::Definition(), e_aq, {{O2, 1.74e10 * M, {O2m}, 0}});

  build(G4O2::Definition(), O2m,
        {{H3OpB, 4.78e10 * M, {HO2}, 6}, {H2O, 0.15 * cW / s, {HO2, OHmB}, 0}});

  build(G4HO2::Definition(), HO2,
        {{OHmB, 1.27e10 * M, {O2m}, 0}, {H2O, 7.58e5 / s, {H3OpB, O2m}, 6}});

  build(G4MoleculeTable::Instance()->GetMoleculeDefinition("HO_2"), HO2m,
        {{H3OpB, 4.78e10 * M, {H2O2}, 0}, {H2O, 1.36e6 * cW / s, {H2O2, OHmB}, 7}});

  build(G4Oxygen::Definition(), Om,
        {{O2, 3.7e9 * M, {O3m}, 0},
         {H3OpB, 9.56e10 * M, {OH}, 0},
         {H2O, 1.8e6 * cW / s, {OH, OHmB}, 8}});

  build(G4O3::Definition(), O3m,
        {{H3OpB, 9.0e10 * M, {OH, O2}, 0}, {H2O, 2.66e3 / s, {Om, O2}, 0}});
}
