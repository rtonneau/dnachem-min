/// \file BoscoloChemReactions.hh
/// \brief BoscoloChem Chemistry: reaction-table and bulk-reaction list builders
///
/// WORK IN PROGRESS: the reaction rates, products and bulk-reaction list in this
/// Chemistry are a verbatim copy of the PureWater Chemistry. Edit them here to
/// reproduce the BoscoloChem network. If this Chemistry omits the acid-base
/// buffer (return an empty list from BuildBoscoloChemBulkReactions), say so here:
/// the buffer is then absent by design, see docs/adr/0002-named-chemistries.md.
///
/// Self-contained, project-agnostic unit: no DnaChemistryWorld, DnaLogger, or
/// other dnachem-min-specific includes -- only standard Geant4/Geant4-DNA
/// headers. Copy-paste portable to another Geant4-DNA project.
///
/// Initial content (copied from PureWaterReactions), added directly to the
/// supplied reaction table:
///   - the 9 base pure-water radiolysis reactions
///   - e_aq/H/O- + O2 against *tracked* radiolytic O2
///     (UHDR: ChemOxygenWaterBuilder::OxygenScavengerReaction)
///   - the O2-/HO2/HO2-/O-/O3- second-order network
///     (UHDR: ChemOxygenWaterBuilder::SecondOrderReactionExtended,
///      O2-relevant subset only -- NO2-/CO2/HCO3-/N2O/MeOH lines excluded,
///      out of project scope)
///
/// Also supplies the pH-driven acid-base buffer equilibria against the bulk
/// H3Op(B)/OHm(B)/H2O pseudo-species and the dissolved-O2 scavenger
/// reactions e_aq/H/O- + O2 (bulk) as plain data, the bulk-reaction list
/// (BuildBoscoloChemBulkReactions);
/// the driver turns that list into G4DNAScavengerProcess registrations.
/// Needs ChemistryTypes.hh -- copy both files to port this unit.
///
/// Hard-coded SBS assumption: no G4ChemTimeStepModel branching, no
/// conditional SetReactionType(1) (that existed only for IRT_syn support,
/// dropped project-wide).

#ifndef BoscoloChemReactions_h
#define BoscoloChemReactions_h 1

#include "chemistry/ChemistryTypes.hh"

class G4DNAMolecularReactionTable;

namespace BoscoloChemReactions
{
  /// Populates reactionTable with the full pure-water + O2 ordinary
  /// (non-bulk) reaction set. Looks up species configurations internally via
  /// G4MoleculeTable::Instance()->GetConfiguration(name); raises a fatal
  /// G4Exception if any expected species is missing. Call after
  /// G4ChemDissociationChannels_option1::ConstructMolecule().
  void BuildBoscoloChemReactions(G4DNAMolecularReactionTable* reactionTable);

  /// The pH-driven acid-base buffer equilibria against the bulk H3Op(B) /
  /// OHm(B) / H2O pseudo-species (UHDR: ChemPureWaterBuilder::
  /// WaterScavengerReaction) plus the dissolved-O2 scavenger reactions
  /// e_aq/H/O- + O2 (bulk, UHDR rates; inert at 0 concentration), as plain
  /// data. Species are named as stored by
  /// G4ChemDissociationChannels_option1; the driver resolves them.
  ChemistryTypes::BulkReactionList BuildBoscoloChemBulkReactions();
}  // namespace BoscoloChemReactions

#endif  // BoscoloChemReactions_h
