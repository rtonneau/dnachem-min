/// \file DnaChemistryList.cc
/// \brief Implementation of the DnaChemistryList class
///
/// Structure mirrors G4EmDNAChemistry_option3 and the UHDR example's
/// EmDNAChemistry. Molecules + water dissociation channels come from
/// G4ChemDissociationChannels_option1. The reaction content (ordinary
/// reaction table + bulk-reaction list) comes from the Chemistry selected with
/// /chem/select before /run/initialize (default PureWater, the portable
/// PureWaterReactions.cc: pure-water radiolysis plus the O2-derived
/// second-order network). See docs/adr/0002-named-chemistries.md.
///
/// The pH-driven acid-base buffer equilibria against the bulk H3Op(B) /
/// OHm(B) pseudo-species (UHDR: ChemPureWaterBuilder::WaterScavengerReaction)
/// are registered as per-molecule G4DNAScavengerProcess from the
/// bulk-reaction data list (PureWaterReactions::BuildPureWaterBulkReactions).
/// The PureWater chemistry always carries the full network -- baseline
/// aqueous chemistry (it can produce O2 from pure water radiolysis on its
/// own), not gated by any scavenger. See
/// docs/adr/0001-baseline-acid-base-buffer.md. The same list holds the
/// scavenger reactions against exogenous bulk species set with
/// /chem/env/scavenger (docs/adr/0004-scavenger-reactions-per-chemistry.md).

#include "chemistry/DnaChemistryList.hh"

#include "chemistry/BuiltInChemistries.hh"
#include "chemistry/ChemistryRegistry.hh"
#include "chemistry/ChemistrySelectMessenger.hh"
#include "chemistry/RateAwareReactionModel.hh"
#include "geometry/DetectorConstruction.hh"
#include "geometry/DnaChemistryWorld.hh"
#include "geometry/ScavengerSpec.hh"
#include "chemistry/ChemistryTypes.hh"
#include "core/DnaLogger.hh"
#include "core/OutputDir.hh"
#include "scoring/ReactionCounter.hh"
#include "chemistry/ReactionTableDump.hh"
#include "chemistry/ScavengerReactionAccess.hh"

#include "G4ApplicationState.hh"
#include "G4ChemDissociationChannels_option1.hh"
#include "G4DNABoundingBox.hh"
#include "G4DNAChemistryManager.hh"
#include "G4DNAMolecularReactionTable.hh"
#include "G4GenericMessenger.hh"
#include "G4Scheduler.hh"
#include "G4MolecularConfiguration.hh"
#include "G4MoleculeDefinition.hh"
#include "G4MoleculeTable.hh"
#include "G4PhysicsListHelper.hh"
#include "G4ProcessManager.hh"
#include "G4ProcessTable.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4Threading.hh"

// Chemical-stage processes
#include "G4DNABrownianTransportation.hh"
#include "G4DNAElectronHoleRecombination.hh"
#include "G4DNAElectronSolvation.hh"
#include "G4DNAMolecularDissociation.hh"
#include "G4DNASancheExcitationModel.hh"
#include "G4DNAScavengerProcess.hh"
#include "G4DNAVibExcitation.hh"
#include "G4DNAWaterDissociationDisplacer.hh"

// Time-step model
#include "G4DNAMolecularStepByStepModel.hh"
#include "G4DNAScavengerMaterial.hh"
#include "G4Scheduler.hh"

// Particles
#include "G4Electron.hh"
#include "G4H2O.hh"

#include "G4PhysicsConstructorFactory.hh"

#include <memory>
#include <vector>

G4_DECLARE_PHYSCONSTR_FACTORY(DnaChemistryList);

namespace
{
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

  // Chemistries are registered before any macro can /chem/select one. The
  // manager singleton above already exists, so /chem/ is there for the commands.
  BuiltInChemistries::Register();
  fSelectMessenger = std::make_unique<ChemistrySelectMessenger>();

  fMessenger = std::make_unique<G4GenericMessenger>(this, "/chem/reaction/",
                                                    "Chemistry reaction-table diagnostics");
  auto& dumpCmd = fMessenger->DeclareProperty(
    "dump", fReactionDumpFile,
    "Write the full reaction table (bimolecular + bulk networks) to <filename>.");
  dumpCmd.SetStates(G4State_PreInit);

  auto& timeBinsFixedCmd = fMessenger->DeclareMethodWithUnit(
    "timeBinsFixed", "picosecond", &DnaChemistryList::SetReactionTimeBinsFixed,
    "Bin the reaction-count output (Reactions.Txt / Reactions_nt_reactions.csv) at this fixed "
    "time step, up to the chemistry scheduler's end time. Mutually exclusive with timeBinsList "
    "(issuing both is a fatal configuration error). Neither issued -> a built-in 7-edge default "
    "table is used.");
  timeBinsFixedCmd.SetStates(G4State_PreInit);

  // DeclareProperty, not DeclareMethod: see fReactionTimeBinsList's doc comment.
  auto& timeBinsListCmd = fMessenger->DeclareProperty(
    "timeBinsList", fReactionTimeBinsList,
    "Bin the reaction-count output at these explicit edges: '<e1> <e2> ... <eN> <unit>' (e.g. "
    "'1 10 100 1000 picosecond'). Mutually exclusive with timeBinsFixed (issuing both is a fatal "
    "configuration error).");
  timeBinsListCmd.SetStates(G4State_PreInit);

  fSbsMessenger = std::make_unique<G4GenericMessenger>(this, "/chem/sbs/",
                                                       "Step-by-step (SBS) chemistry options");
  // DeclareMethod, not DeclareProperty: a bool property is assigned through
  // G4AnyType::FromString (stream >> bool, which rejects "true"), while the
  // method path converts the token with G4UIcommand's StoB.
  auto& rateAwareCmd = fSbsMessenger->DeclareMethod(
    "rateAwareReactions", &DnaChemistryList::SetRateAwareReactions,
    "true: accept along-step encounters with the exact 3D encounter probability, so slow "
    "reactions fire at their rate constant (RateAwareReactionModel). false (default): Geant4's "
    "G4DNASmoluchowskiReactionModel. See docs/adr/0006-opt-in-sbs-rate-aware-acceptance.md.");
  rateAwareCmd.SetStates(G4State_PreInit);

  auto& maxStepCmd = fSbsMessenger->DeclareMethodWithUnit(
    "maxTimeStep", "ns", &DnaChemistryList::SetMaxTimeStep,
    "Upper bound on the chemistry time step (G4Scheduler::SetMaxTimeStep). Must be > 0. Not "
    "issued = no cap. The scheduler's own minimum steps are unchanged.");
  maxStepCmd.SetStates(G4State_PreInit);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DnaChemistryList::~DnaChemistryList() = default;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

const ChemistryRegistry::Chemistry* DnaChemistryList::SelectedChemistry(
  const G4String& caller) const
{
  const auto* chemistry = ChemistryRegistry::Selected();
  if (chemistry == nullptr) {
    G4Exception((G4String("DnaChemistryList::") + caller).c_str(), "NoChemistry", FatalException,
                "No chemistry is selected and the default is not registered.");
  }
  return chemistry;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::SetReactionTimeBinsFixed(G4double width)
{
  fReactionBinWidthSet = true;
  fReactionBinWidth = width;
}

void DnaChemistryList::SetRateAwareReactions(G4bool enabled)
{
  fRateAwareReactions = enabled;
}

void DnaChemistryList::SetMaxTimeStep(G4double maxTimeStep)
{
  if (!(maxTimeStep > 0.)) {
    G4Exception("DnaChemistryList::SetMaxTimeStep", "BadMaxTimeStep", FatalException,
                "/chem/sbs/maxTimeStep must be > 0.");
  }
  fMaxTimeStep = maxTimeStep;
}

void DnaChemistryList::ApplyMaxTimeStep() const
{
  if (fMaxTimeStep == DBL_MAX) return;
  G4Scheduler::Instance()->SetMaxTimeStep(fMaxTimeStep);
  DnaLogger::Print(DnaLogger::Level::Info, "[DnaChemistryList] chemistry max time step = " +
                                             std::to_string(fMaxTimeStep / ns) + " ns");
}

void DnaChemistryList::ApplyReactionTimeBinning() const
{
  const G4bool listRequested = !fReactionTimeBinsList.empty();
  if (fReactionBinWidthSet && listRequested) {
    G4Exception("DnaChemistryList::ApplyReactionTimeBinning", "ConflictingTimeBins", FatalException,
                "/chem/reaction/timeBinsFixed and /chem/reaction/timeBinsList were both issued -- "
                "use only one per run.");
    return;
  }

  if (fReactionBinWidthSet) {
    const G4double endTime = G4Scheduler::Instance()->GetEndTime();
    std::vector<G4double> edges;
    for (G4double edge = fReactionBinWidth; edge < endTime + fReactionBinWidth;
         edge += fReactionBinWidth) {
      edges.push_back(edge);
    }
    ReactionCounter::ConfigureBinEdges(edges);
    return;
  }

  if (listRequested) {
    std::vector<G4double> edges;
    G4String error;
    if (!ReactionCounter::ParseBinEdgesList(fReactionTimeBinsList, edges, error)) {
      G4Exception("DnaChemistryList::ApplyReactionTimeBinning", "InvalidTimeBinsList", FatalException,
                  error.c_str());
      return;
    }
    ReactionCounter::ConfigureBinEdges(edges);
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
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::ConstructDissociationChannels()
{
  G4ChemDissociationChannels_option1::ConstructDissociationChannels();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::ConstructReactionTable(G4DNAMolecularReactionTable* reactionTable)
{
  // Populate the chemistry-world bulk composition here (master-only phase ->
  // race-free), before it is read by G4DNAScavengerMaterial.
  auto* chemWorld =
    const_cast<DnaChemistryWorld*>(ChemistryWorld("ConstructReactionTable"));
  chemWorld->ConstructChemistryComponents();

  // A scavenger whose species no bulk reaction of the selected Chemistry uses
  // would sit in the bulk without reacting. Master-only, so it warns once.
  const auto* chemistry = SelectedChemistry("ConstructReactionTable");
  for (const auto& species :
       ScavengerSpec::InertSpecies(chemWorld->GetScavengers(), chemistry->buildBulkReactions())) {
    DnaLogger::Print(DnaLogger::Level::Warning,
                     "[DnaChemistryList] scavenger " + species +
                       " is inert: no bulk reaction of chemistry " + chemistry->name +
                       " uses it as a partner");
  }

  // Ordinary (non-bulk) reactions of the selected Chemistry (default PureWater).
  chemistry->buildReactions(reactionTable);

  DnaLogger::Print(DnaLogger::Level::Info,
                   "[DnaChemistryList] chemistry = " + chemistry->name +
                     ", reaction table constructed");
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::ConstructTimeStepModel(G4DNAMolecularReactionTable* /*reactionTable*/)
{
  if (!fRateAwareReactions) {
    RegisterTimeStepModel(new G4DNAMolecularStepByStepModel(), 0);
    DnaLogger::Print(DnaLogger::Level::Info,
                     "[DnaChemistryList] time-step model = SBS (hard-coded), "
                     "reaction acceptance = Geant4 Brownian bridge (G4DNASmoluchowskiReactionModel)");
    return;
  }

  // Geant4 11.4.1 has no SBS constructor taking a reaction model; a model set
  // before Initialize() is kept (G4DNAMolecularStepByStepModel.cc:69-74), and
  // the SBS model owns it.
  auto* sbs = new G4DNAMolecularStepByStepModel();
  sbs->SetReactionModel(new RateAwareReactionModel());
  RegisterTimeStepModel(sbs, 0);
  DnaLogger::Print(DnaLogger::Level::Info,
                   "[DnaChemistryList] time-step model = SBS (hard-coded), "
                   "reaction acceptance = rate-aware (RateAwareReactionModel)");
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::ConstructProcess()
{
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

  auto* moleculeTable = G4MoleculeTable::Instance();
  auto iterator = moleculeTable->GetDefintionIterator();
  iterator.reset();
  while (iterator()) {
    auto* moleculeDef = iterator.value();

    if (moleculeDef != G4H2O::Definition()) {
      // SBS is the only supported time-step model; always transport.
      ph->RegisterProcess(new G4DNABrownianTransportation(), moleculeDef);
    }
    else {
      moleculeDef->GetProcessManager()->AddRestProcess(new G4DNAElectronHoleRecombination(), 2);
      auto* dissociation = new G4DNAMolecularDissociation("H2O_DNAMolecularDecay");
      dissociation->SetDisplacer(moleculeDef, new G4DNAWaterDissociationDisplacer);
      moleculeDef->GetProcessManager()->AddRestProcess(dissociation, 1);
    }
  }

  auto* chemWorld = const_cast<DnaChemistryWorld*>(ChemistryWorld("ConstructProcess"));
  RegisterBulkReactionProcesses(*chemWorld->GetChemistryBoundary(),
                                SelectedChemistry("ConstructProcess")->buildBulkReactions());

  // Triggers InitializeMaster() -> ConstructReactionTable() ->
  // DnaChemistryWorld::ConstructChemistryComponents(): the bulk composition is
  // populated after this call returns.
  G4DNAChemistryManager::Instance()->Initialize();

  // Install the bulk-scavenger material now that the composition is known, and
  // before G4DNAScavengerProcess::BuildPhysicsTable() queries the scheduler.
  // Runs once (serial) or per worker thread (MT); the scheduler is thread-local.
  // Unconditional: the bulk-reaction scavenger processes registered above
  // are always active, so this material must always be installed -- without
  // it, G4DNAScavengerProcess::PostStepGetPhysicalInteractionLength would
  // dereference a null fpScavengerMaterial on the first step of any species
  // with a registered reaction.
  if (G4Scheduler::Instance()->GetScavengerMaterial() == nullptr) {
    auto scavenger = std::make_unique<G4DNAScavengerMaterial>(chemWorld);
    scavenger->SetCounterAgainstTime();
    G4Scheduler::Instance()->SetScavengerMaterial(std::move(scavenger));
  }

  // Both networks (bimolecular + bulk) are fully constructed by this
  // point; opt-in dump for external checks (see /chem/reaction/dump).
  // Dump is written once, from the master (MT) or the only thread (Serial).
  if (!fReactionDumpFile.empty() && !G4Threading::IsWorkerThread()) {
    ReactionTableDump::DumpReactionTable(OutputDir::Resolve(fReactionDumpFile));
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::RegisterBulkReactionProcesses(
  const G4DNABoundingBox& boundary, const ChemistryTypes::BulkReactionList& list) const
{
  auto* ph = G4PhysicsListHelper::GetPhysicsListHelper();
  const G4String caller = "RegisterBulkReactionProcesses";

  // Registers against mol->GetDefinition() rather than a caller-supplied
  // class pointer: several species here (OHm, H3Op) have no dedicated
  // factory class, and using GetDefinition() uniformly avoids the previous
  // mismatch where Om's process was attached to G4Oxygen::Definition() (the
  // separate "Oxy" O(3P) species) instead of Om's actual definition (a
  // private G4MoleculeDefinition("O", ...) created inside
  // G4ChemDissociationChannels_option1::ConstructMolecule()) -- silently
  // making those reactions unreachable.
  for (const auto& entry : list) {
    auto* mol = Conf(entry.molecule, caller);
    auto* process = new ScavengerReactionAccess("G4DNAScavengerProcess", boundary);
    for (const auto& r : entry.reactions) {
      auto* rd = new G4DNAMolecularReactionData(r.rate, mol, Conf(r.partner, caller));
      for (const auto& product : r.products) {
        rd->AddProduct(Conf(product, caller));
      }
      if (r.reactionType != 0) {
        rd->SetReactionType(r.reactionType);
      }
      process->SetReaction(mol, rd);
    }
    auto* def = const_cast<G4MoleculeDefinition*>(mol->GetDefinition());
    ph->RegisterProcess(process, def);
  }
}
