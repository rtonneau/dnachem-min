/// \file ManifestWriter.hh
/// \brief Portable, stream-only JSON serialiser for a dump's manifest.
///
/// Self-contained, project-agnostic unit: standard library plus
/// ManifestData.hh only -- no Geant4, OutputDir or DnaLogger. Copy-paste
/// portable to another Geant4-DNA project (adjust the include path prefix on
/// copy). The caller opens the file and fills the ManifestData::Manifest.

#ifndef ManifestWriter_h
#define ManifestWriter_h 1

#include "scoring/ManifestData.hh"

#include <iosfwd>
#include <string>

namespace ManifestWriter
{
  /// Escapes text for use inside a JSON string: `"`, `\`, newline, carriage
  /// return and tab get their short escapes, other control characters
  /// (< 0x20) become \u00XX.
  std::string EscapeJson(const std::string &text);

  /// Writes `manifest` as 2-space-indented JSON. `totalEvents` and
  /// `totalEnergyDeposit_eV` are summed from `manifest.runs`. Doubles print
  /// with 12 significant digits; a non-finite double prints null. A run
  /// without a beam prints null for its beam fields.
  void Write(std::ostream &out, const ManifestData::Manifest &manifest);
} // namespace ManifestWriter

#endif // ManifestWriter_h
