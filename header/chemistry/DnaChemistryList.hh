/// \file DnaChemistryList.hh
/// \brief Definition of the DnaChemistryList class
///
/// Project chemical stage, replacing the macro-driven `/chem/species` +
/// `/chem/reaction/add` blocks. Inherits BOTH `G4VUserChemistryList`
/// (molecules / reactions / time-step model) and `G4VPhysicsConstructor`
/// (driven directly by PhysicsList, UHDR-style).
///
/// Reaction content: the Chemistry chosen with `/chem/select <name>` before
/// `/run/initialize` (default `PureWater`; see ChemistryRegistry). It supplies
/// the ordinary reaction table and the bulk-reaction list (reactions against
/// bulk species: the pH-driven acid-base buffer against H3Op(B)/OHm(B), UHDR:
/// ChemPureWaterBuilder), registered as per-molecule `G4DNAScavengerProcess`.
/// `PureWater` is the portable pure-water + O2-derived network
/// (PureWaterReactions.cc) with the full acid-base network; another Chemistry
/// may omit that buffer
/// (docs/adr/0002-named-chemistries.md).
/// Exogenous scavengers (e.g. dissolved O2, `/chem/env/scavenger` on
/// DnaChemistryWorld) react through the Chemistry's bulk reactions
/// (docs/adr/0004-scavenger-reactions-per-chemistry.md).
///
/// Time-step model: SBS only (hard-coded; IRT and IRT_syn are not
/// supported). `/chem/sbs/rateAwareReactions true` swaps its reaction model
/// for RateAwareReactionModel (docs/adr/0006-opt-in-sbs-rate-aware-acceptance.md).

#ifndef DnaChemistryList_h
#define DnaChemistryList_h 1

#include "chemistry/ChemistryTypes.hh"

#include "G4VPhysicsConstructor.hh"
#include "G4VUserChemistryList.hh"
#include "globals.hh"

#include <cfloat>
#include <memory>

namespace ChemistryRegistry
{
struct Chemistry;
}
class ChemistrySelectMessenger;
class G4DNABoundingBox;
class G4DNAMolecularReactionTable;
class G4GenericMessenger;
class DnaChemistryWorld;

class DnaChemistryList : public G4VUserChemistryList, public G4VPhysicsConstructor
{
public:
  DnaChemistryList();
  ~DnaChemistryList() override;

  // --- G4VPhysicsConstructor ---
  void ConstructParticle() override { ConstructMolecule(); }
  void ConstructProcess() override;

  // --- G4VUserChemistryList ---
  void ConstructMolecule() override;
  void ConstructDissociationChannels() override;
  void ConstructReactionTable(G4DNAMolecularReactionTable* reactionTable) override;
  void ConstructTimeStepModel(G4DNAMolecularReactionTable* reactionTable) override;

  /// Resolves any /chem/reaction/timeBinsFixed or /chem/reaction/timeBinsList
  /// macro command into ReactionCounter::ConfigureBinEdges() -- a no-op if
  /// neither was issued (ReactionCounter keeps its built-in default table).
  /// Must be called after the chemistry scheduler's end time is final (i.e.
  /// from RunAction::BeginOfRunAction, not any earlier), since the "fixed"
  /// mode expands its step into edges up to that end time. Raises a
  /// FatalException if both commands were issued, or if timeBinsList's value
  /// is malformed (parsing is deferred to here, not macro-issue time, since
  /// timeBinsList is a plain string property -- see fReactionTimeBinsList).
  void ApplyReactionTimeBinning() const;

  /// True when /chem/sbs/rateAwareReactions true was issued: the SBS model
  /// then uses RateAwareReactionModel instead of Geant4's default.
  G4bool IsRateAwareReactions() const { return fRateAwareReactions; }

  /// /chem/sbs/maxTimeStep cap (internal time units); DBL_MAX when unset.
  G4double GetMaxTimeStep() const { return fMaxTimeStep; }

  /// Calls G4Scheduler::Instance()->SetMaxTimeStep(cap) only when a cap was
  /// set. G4Scheduler is thread-local, so this must run on every thread that
  /// runs chemistry (RunAction::BeginOfRunAction), not in the PreInit command.
  void ApplyMaxTimeStep() const;

private:
  /// /chem/sbs/maxTimeStep <value> <unit> setter (fatal if <= 0).
  void SetMaxTimeStep(G4double maxTimeStep);

  /// /chem/sbs/rateAwareReactions <bool> setter.
  void SetRateAwareReactions(G4bool enabled);

  /// /chem/reaction/timeBinsFixed <width> <unit> setter.
  void SetReactionTimeBinsFixed(G4double width);

  /// The Chemistry chosen with /chem/select (default PureWater); fatal if
  /// none is available.
  const ChemistryRegistry::Chemistry* SelectedChemistry(const G4String& caller) const;

  /// The project chemistry world, via the run manager's detector.
  const DnaChemistryWorld* ChemistryWorld(const G4String& caller) const;

  /// Registers one G4DNAScavengerProcess per entry of `list`: the bulk
  /// reactions, e.g. the pH-driven acid-base buffer equilibria against the
  /// bulk H3Op(B) / OHm(B) / H2O pseudo-species (UHDR:
  /// ChemPureWaterBuilder::WaterScavengerReaction).
  /// The values come from the Chemistry (PureWater: always the full network);
  /// an empty list registers nothing.
  void RegisterBulkReactionProcesses(const G4DNABoundingBox& boundary,
                                     const ChemistryTypes::BulkReactionList& list) const;

  /// Exposes /chem/select <name> and /chem/list.
  std::unique_ptr<ChemistrySelectMessenger> fSelectMessenger;

  /// Exposes /chem/reaction/dump <filename>.
  std::unique_ptr<G4GenericMessenger> fMessenger;

  /// Exposes /chem/sbs/rateAwareReactions (own directory: G4DNAChemistryManager
  /// owns /chem/).
  std::unique_ptr<G4GenericMessenger> fSbsMessenger;

  /// /chem/sbs/rateAwareReactions; false (default) keeps Geant4's
  /// G4DNASmoluchowskiReactionModel. Set in PreInit on this shared object,
  /// before worker threads build their time-step models.
  G4bool fRateAwareReactions = false;

  /// /chem/sbs/maxTimeStep; DBL_MAX (default) = no cap.
  G4double fMaxTimeStep = DBL_MAX;

  /// Target file for ConstructProcess() to dump the reaction table to;
  /// empty (default) disables the dump.
  G4String fReactionDumpFile;

  /// True once /chem/reaction/timeBinsFixed has been issued.
  G4bool fReactionBinWidthSet = false;

  /// Set by timeBinsFixed; only meaningful when fReactionBinWidthSet.
  G4double fReactionBinWidth = 0.;

  /// Raw /chem/reaction/timeBinsList value, e.g. "1 10 100 picosecond";
  /// empty (default) means the command wasn't issued. A DeclareProperty
  /// (not DeclareMethod): G4GenericMessenger's method dispatch re-tokenizes
  /// a combined multi-token command value by the bound function's argument
  /// count, truncating a "<e1> ... <eN> <unit>" string to just its first
  /// token; DeclareProperty's G4String::FromString() assigns it intact.
  /// Parsed lazily by ApplyReactionTimeBinning().
  G4String fReactionTimeBinsList;
};

#endif // DnaChemistryList_h
