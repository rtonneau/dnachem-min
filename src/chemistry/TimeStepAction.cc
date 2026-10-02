/// \file TimeStepAction.cc
/// \brief Implementation of the TimeStepAction class

#include "chemistry/TimeStepAction.hh"

#include "core/DnaLogger.hh"
#include "chemistry/MesoSettings.hh"
#include "chemistry/ReactionTableDump.hh"

#include "G4DNABoundingBox.hh"
#include "G4DNAEventScheduler.hh"
#include "G4DNAMesh.hh"
#include "G4DNAMolecularReactionTable.hh"
#include "G4DNAScavengerMaterial.hh"
#include "G4ITTrackHolder.hh"
#include "G4MolecularConfiguration.hh"
#include "G4Molecule.hh"
#include "G4MoleculeCounterManager.hh"
#include "G4Scheduler.hh"
#include "G4SystemOfUnits.hh"
#include "G4Timer.hh"
#include "G4UserMeshAction.hh"
#include "G4VChemistryWorld.hh"

#include <cmath>
#include <sstream>
#include <utility>
#include <vector>

namespace
{
/// Number of molecules held by a mesoscopic mesh (all cells, all species).
long long MeshTotal(const G4DNAMesh &mesh)
{
  long long total = 0;
  for (auto it = mesh.const_begin(); it != mesh.const_end(); ++it) {
    for (const auto &[molType, count] : std::get<2>(*it)) {
      total += static_cast<long long>(count);
    }
  }
  return total;
}

/// Pixels per side of a mesh, from its bounding box and cell size.
long long MeshPixels(const G4DNAMesh &mesh)
{
  return std::llround(2. * mesh.GetBoundingBox().halfSideLengthInY() / mesh.GetResolution());
}

G4String Format(G4double value)
{
  std::ostringstream os;
  os.precision(6);
  os << value;
  return os.str();
}

/// Molecule count of every species held by G4DNAScavengerMaterial (the bulk:
/// H2O(B), H3Op(B), OHm(B) and the /chem/env/scavenger species), in its
/// scavenger-table order. Its bulk reactions change these counts, so two
/// snapshots show what the mesoscopic stage consumed or produced in bulk.
std::vector<std::pair<const G4MolecularConfiguration *, long long>> BulkCounts()
{
  std::vector<std::pair<const G4MolecularConfiguration *, long long>> counts;
  auto *scavenger =
    dynamic_cast<G4DNAScavengerMaterial *>(G4Scheduler::Instance()->GetScavengerMaterial());
  if (scavenger == nullptr) return counts;
  for (const auto *conf : scavenger->GetScavengerList()) {
    counts.emplace_back(conf, static_cast<long long>(
                                scavenger->GetNumberMoleculePerVolumeUnitForMaterialConf(conf)));
  }
  return counts;
}

/// Debug-level molecule bookkeeping of the mesoscopic stage. A diffusive jump
/// never changes the mesh total and one reaction changes it by at most 2
/// (A + B -> nothing), so between two Gillespie steps the total may only move
/// by 0, +-1 or +-2; anything else, or a change across a mesh coarsening
/// (EndOfMesh -> next BeginOfMesh, no step in between), is a drift.
class MeshTotalsAction : public G4UserMeshAction
{
public:
  void ResetEvent(long long handOverTotal)
  {
    fLastTotal = handOverTotal;
    fDrifts = 0;
    fMeshChanges = 0;
    fFirstMesh = true;
  }

  void BeginOfMesh(const G4VDNAMesh *aMesh, G4double time) override
  {
    if (!DnaLogger::Enabled(DnaLogger::Level::Debug)) return;
    const auto *mesh = dynamic_cast<const G4DNAMesh *>(aMesh);
    if (mesh == nullptr) return;
    const long long total = MeshTotal(*mesh);
    const G4bool conserved = (total == fLastTotal);
    if (!conserved) ++fDrifts;
    if (!fFirstMesh) ++fMeshChanges;
    DnaLogger::Print(DnaLogger::Level::Debug,
                     G4String("[meso] ") + (fFirstMesh ? "first mesh" : "mesh change") +
                       " at t = " + Format(time / ns) + " ns: pixels = " +
                       std::to_string(MeshPixels(*mesh)) + ", cell = " +
                       Format(mesh->GetResolution() / nm) + " nm, occupied cells = " +
                       std::to_string(mesh->const_end() - mesh->const_begin()) +
                       ", total = " + std::to_string(total) + " (previous " +
                       std::to_string(fLastTotal) + (conserved ? ", conserved)" : ", DRIFT)"));
    fFirstMesh = false;
    fLastTotal = total;
    fMeshStartTotal = total;
    fSteps = 0;
    fChangedSteps = 0;
    fAnomalousSteps = 0;
  }

  void InMesh(const G4VDNAMesh *aMesh, G4double /*time*/) override
  {
    if (!DnaLogger::Enabled(DnaLogger::Level::Debug)) return;
    const auto *mesh = dynamic_cast<const G4DNAMesh *>(aMesh);
    if (mesh == nullptr) return;
    const long long total = MeshTotal(*mesh);
    const long long delta = total - fLastTotal;
    ++fSteps;
    if (delta != 0) ++fChangedSteps;
    if (delta > 2 || delta < -2) {
      ++fAnomalousSteps;
      ++fDrifts;
    }
    fLastTotal = total;
  }

  void EndOfMesh(const G4VDNAMesh *aMesh, G4double time) override
  {
    if (!DnaLogger::Enabled(DnaLogger::Level::Debug)) return;
    const auto *mesh = dynamic_cast<const G4DNAMesh *>(aMesh);
    if (mesh == nullptr) return;
    const long long total = MeshTotal(*mesh);
    DnaLogger::Print(DnaLogger::Level::Debug,
                     "[meso] end of mesh at t = " + Format(time / ns) +
                       " ns: pixels = " + std::to_string(MeshPixels(*mesh)) + ", total = " +
                       std::to_string(total) + " (start " + std::to_string(fMeshStartTotal) +
                       "), steps = " + std::to_string(fSteps) +
                       ", reaction steps changing the total = " +
                       std::to_string(fChangedSteps) +
                       ", steps with |delta| > 2 = " + std::to_string(fAnomalousSteps));
    fLastTotal = total;
  }

  long long Drifts() const { return fDrifts; }
  long long MeshChanges() const { return fMeshChanges; }
  long long LastTotal() const { return fLastTotal; }

private:
  long long fLastTotal = 0;
  long long fMeshStartTotal = 0;
  long long fSteps = 0;
  long long fChangedSteps = 0;
  long long fAnomalousSteps = 0;
  long long fDrifts = 0;
  long long fMeshChanges = 0;
  G4bool fFirstMesh = true;
};

/// Thread-local: one TimeStepAction (and so one mesh action) per thread. The
/// scheduler owns the action; this is a non-owning view for the summary.
G4ThreadLocal MeshTotalsAction *tMeshAction = nullptr;
}  // namespace

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

TimeStepAction::TimeStepAction(const G4VChemistryWorld *chemistryWorld)
  : G4UserTimeStepAction(),
    fpChemWorld(chemistryWorld),
    fpEventScheduler(std::make_unique<G4DNAEventScheduler>())
{
  // No AddTimeStep() user time steps: the IRT_syn stepper ignores the
  // scheduler's defined minimum time step (G4DNAIndependentReactionTimeStepper
  // keeps its own fUserMinTimeStep), as in the UHDR example.
  if (fpChemWorld == nullptr) {
    G4Exception("TimeStepAction::TimeStepAction", "NoChemistryWorld", FatalException,
                "The mesoscopic stage needs the chemistry world (its boundary is the mesh).");
  }
  auto meshAction = std::make_unique<MeshTotalsAction>();
  tMeshAction = meshAction.get();
  fpEventScheduler->SetUserMeshAction(std::move(meshAction));
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

TimeStepAction::~TimeStepAction()
{
  tMeshAction = nullptr;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void TimeStepAction::StartProcessing()
{
  fHandedOver = false;
  fpEventScheduler->SetVerbose(G4Scheduler::Instance()->GetVerbose());

  // G4DNAEventScheduler::RecordTime / LastRegisterForCounter dereference the
  // record-time iterator unconditionally, so at least one record time is
  // required. Log grid from the hand-over time to the end time
  // (/chem/meso/timesPerDecade); ResetCounter rewinds it. When the end time is
  // not after the hand-over time there is no mesoscopic stage and the grid
  // reduces to the end time alone.
  const auto& meso = MesoSettings::Current();
  const G4double handOverTime = meso.handOverTime * ns;
  const G4double endTime = G4Scheduler::Instance()->GetEndTime();
  if (handOverTime < endTime) {
    for (const G4double t : MesoSettings::LogTimeGrid(handOverTime, endTime, meso.timesPerDecade)) {
      fpEventScheduler->AddTimeToRecord(t);
    }
  }
  else {
    fpEventScheduler->AddTimeToRecord(endTime);
  }
  fpEventScheduler->ResetCounter();
  fHandOverDrift = false;
  fParticleStageWall = 0.;
  fChemTimer.Start();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void TimeStepAction::UserPreTimeStepAction()
{
  // Particle-stage entries of the scheduler's record-time counter (UHDR).
  fpEventScheduler->ParticleBasedCounter();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void TimeStepAction::UserPostTimeStepAction()
{
  if (!fHandedOver && G4Scheduler::Instance()->GetGlobalTime() >= MesoSettings::Current().handOverTime * ns) {
    fHandedOver = true;
    CompartmentBased();
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4int TimeStepAction::InitialPixel() const
{
  const G4double side = 2. * fpChemWorld->GetChemistryBoundary()->halfSideLengthInX();
  return MesoSettings::PixelCount(side, MesoSettings::Current().voxelSize * mm);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void TimeStepAction::CompartmentBased()
{
  const G4double globalTime = G4Scheduler::Instance()->GetGlobalTime();
  const G4int pixel = InitialPixel();
  const G4bool debug = DnaLogger::Enabled(DnaLogger::Level::Debug);

  long long handOverTotal = 0;
  if (debug) {
    // Same selection as G4DNAEventScheduler::Voxelizing: every main-list
    // track except species held by the scavenger material.
    auto *scavenger =
      dynamic_cast<G4DNAScavengerMaterial *>(G4Scheduler::Instance()->GetScavengerMaterial());
    long long listed = 0;
    long long killed = 0;
    for (auto *track : *G4ITTrackHolder::Instance()->GetMainList()) {
      auto *molType = GetMolecule(track)->GetMolecularConfiguration();
      if (scavenger != nullptr && scavenger->find(molType)) continue;
      ++listed;
      if (track->GetTrackStatus() == fStopAndKill) ++killed;
    }
    handOverTotal = listed;
    const G4double nextTime = G4ITTrackHolder::Instance()->GetNextTime();
    DnaLogger::Print(DnaLogger::Level::Debug,
                     "[meso] hand-over at t = " + Format(globalTime / ns) +
                       " ns: particle-stage total = " + std::to_string(listed) + " (" +
                       std::to_string(killed) + " already killed), initial pixels = " +
                       std::to_string(pixel) + ", cell = " +
                       Format(2. * fpChemWorld->GetChemistryBoundary()->halfSideLengthInX() /
                              pixel / nm) +
                       " nm, delayed tracks pending: " +
                       (nextTime == DBL_MAX ? G4String("none")
                                            : "next at " + Format(nextTime / ns) + " ns"));
  }
  fChemTimer.Stop();
  fParticleStageWall = fChemTimer.GetRealElapsed();
  fChemTimer.Start();

  // The G4MoleculeCounter (Species.*) covers the particle-based stage only,
  // so it is muted from here to EndProcessing (the manager is thread-local:
  // this only affects this thread; the UHDR example has no counter). Two
  // calls would otherwise trip its time-consistency check (TIME_DONT_MATCH):
  // - G4DNAUpdateSystemModel feeds it at the mesoscopic time while
  //   G4Scheduler's global time stays at the hand-over;
  // - the tracks Voxelizing marks fStopAndKill are deleted on the scheduler's
  //   next step, once its global time has jumped to the end time, and
  //   ~G4Molecule removes them at their own (hand-over) time.
  auto *counterManager = G4MoleculeCounterManager::Instance();
  fCounterWasActive = counterManager->GetIsActive();
  fCounterMuted = true;
  counterManager->SetIsActive(false);

  // UHDR example, TimeStepAction::CompartmentBased.
  fpEventScheduler->SetStartTime(globalTime);  // continue from the global time
  fpEventScheduler->SetChangeMesh(true);
  fpEventScheduler->Initialize(*fpChemWorld->GetChemistryBoundary(), pixel);

  if (debug) {
    const auto *mesh = fpEventScheduler->GetMesh();
    const long long total = MeshTotal(*mesh);
    DnaLogger::Print(DnaLogger::Level::Debug,
                     "[meso] after Initialize: pixels = " +
                       std::to_string(fpEventScheduler->GetPixels()) + ", cell = " +
                       Format(mesh->GetResolution() / nm) + " nm, occupied cells = " +
                       std::to_string(mesh->const_end() - mesh->const_begin()) +
                       ", mesh total = " + std::to_string(total) +
                       (total == handOverTotal ? " (conserved)" : " (DRIFT)") +
                       ", meso end time = " + Format(fpEventScheduler->GetEndTime() / ns) +
                       " ns");
    // The hand-over itself must not change the total; the mesh action then
    // tracks the mesh from this total on.
    fHandOverDrift = (total != handOverTotal);
    if (tMeshAction != nullptr) tMeshAction->ResetEvent(total);
  }

  const auto bulkAtHandOver = debug ? BulkCounts() : decltype(BulkCounts()){};

  fpEventScheduler->Run();

  if (debug) {
    // Net change of each bulk species over the mesoscopic stage (reactions
    // against a bulk partner consume it; bulk products, e.g. O2 from
    // O3- + H3O+(B) with bulk O2 set, add to it). H2O(B) / H3Op(B) / OHm(B)
    // stay constant: G4DNAScavengerMaterial holds the pH fixed.
    for (const auto &[conf, after] : BulkCounts()) {
      long long before = after;
      for (const auto &[confAtHandOver, n] : bulkAtHandOver) {
        if (confAtHandOver == conf) before = n;
      }
      DnaLogger::Print(DnaLogger::Level::Debug,
                       "[meso] bulk " + conf->GetName() + ": " + std::to_string(before) +
                         " at hand-over -> " + std::to_string(after) +
                         " at end of mesoscopic stage (net " + std::to_string(after - before) +
                         ")");
    }
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

// Counts each bimolecular reaction that fires, binned by time, so a
// per-reaction rate-over-time output can be written at end of run (see
// Run::Merge / RunAction::EndOfRunAction). Only the particle-based stage
// reports reactions; the mesoscopic stage has no reaction hook.
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
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void TimeStepAction::EndProcessing()
{
  fChemTimer.Stop();
  if (DnaLogger::Enabled(DnaLogger::Level::Info)) {
    const G4double stageWall = fChemTimer.GetRealElapsed();
    G4String summary = "[TimeStepAction] Chemistry ends at global time " +
                       Format(G4Scheduler::Instance()->GetGlobalTime() / picosecond) +
                       " ps, wall time " + Format(fParticleStageWall + stageWall) + " s";
    if (fHandedOver) {
      summary += " (particle stage " + Format(fParticleStageWall) + " s, meso " +
                 Format(stageWall) + " s)";
      if (DnaLogger::Enabled(DnaLogger::Level::Debug) && tMeshAction != nullptr) {
        const G4bool ok = !fHandOverDrift && tMeshAction->Drifts() == 0;
        summary += "; meso: " + std::to_string(tMeshAction->MeshChanges()) +
                   " mesh changes, final total = " + std::to_string(tMeshAction->LastTotal()) +
                   ", conservation " + (ok ? "OK" : "DRIFT");
      }
    }
    DnaLogger::Print(DnaLogger::Level::Info, summary);
  }
  if (fCounterMuted) {
    // The killed particle-stage tracks are gone by now (see CompartmentBased).
    G4MoleculeCounterManager::Instance()->SetIsActive(fCounterWasActive);
    fCounterMuted = false;
  }
  fpEventScheduler->Reset();  // also fills the record times not reached
  if (DnaLogger::Enabled(DnaLogger::Level::Debug)) {
    for (const auto &[time, counts] : fpEventScheduler->GetCounterMap()) {
      long long total = 0;
      for (const auto &[molType, n] : counts) total += n;
      DnaLogger::Print(DnaLogger::Level::Debug, "[meso] record t = " + Format(time / ns) +
                                                  " ns: total = " + std::to_string(total));
    }
  }
  fHandedOver = false;
  fHandOverDrift = false;
}
