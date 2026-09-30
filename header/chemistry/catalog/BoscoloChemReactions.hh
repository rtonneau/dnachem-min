/// \file BoscoloChemReactions.hh
/// \brief BoscoloChem Chemistry: reaction-table and bulk-reaction list builders
///
/// Reproduces Table 1 of Boscolo et al., "Impact of Target Oxygenation on the
/// Chemical Track Evolution of Ion and Electron Radiation", Int. J. Mol. Sci.
/// 2020, 21, 424: reactions (i)-(xxvi) with the table's rate constants, H2O
/// reactants/products dropped (water is not tracked).
///
/// Self-contained, project-agnostic unit: no DnaChemistryWorld, DnaLogger, or
/// other dnachem-min-specific includes -- only standard Geant4/Geant4-DNA
/// headers. Copy-paste portable to another Geant4-DNA project.
///
/// The paper treats dissolved O2 as a continuum while (xvi), (xvii), (xxv) and
/// (xxvi) produce O2, so (xiv) e_aq + O2 and (xv) H + O2 appear twice: in the
/// reaction table (against *tracked* radiolytic O2) and in the bulk-reaction
/// list (against the dissolved-O2 background set by /chem/env/scavenger O2),
/// with the same rates. The bulk-reaction list holds nothing else.
///
/// No acid-base buffer, by design: the paper has no bulk H3Op(B)/OHm(B)
/// equilibria (see docs/adr/0002-named-chemistries.md), so /chem/env/pH has no
/// chemical effect under this Chemistry. O- and O3- stay in the shared
/// molecule set but take part in no reaction. The paper's Table 2 diffusion
/// coefficients are not applied: molecules are shared by all Chemistries.
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
  /// Populates reactionTable with the 26 Table 1 reactions between tracked
  /// molecules. Looks up species configurations internally via
  /// G4MoleculeTable::Instance()->GetConfiguration(name); raises a fatal
  /// G4Exception if any expected species is missing. Call after
  /// G4ChemDissociationChannels_option1::ConstructMolecule().
  void BuildBoscoloChemReactions(G4DNAMolecularReactionTable* reactionTable);

  /// The dissolved-O2 bulk reactions (xiv) e_aq + O2 -> O2- and (xv) H + O2 ->
  /// HO2 (inert at 0 concentration), as plain data. No acid-base buffer.
  /// Species are named as stored by G4ChemDissociationChannels_option1; the
  /// driver resolves them.
  ChemistryTypes::BulkReactionList BuildBoscoloChemBulkReactions();
}  // namespace BoscoloChemReactions

#endif  // BoscoloChemReactions_h
