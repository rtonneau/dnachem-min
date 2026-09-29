/// \file ManifestData.hh
/// \brief Plain data describing one dump's manifest (see ManifestWriter).
///
/// Self-contained, project-agnostic unit: standard library only, no Geant4
/// or other dnachem-min includes. Copy-paste portable to another Geant4-DNA
/// project (adjust the include path prefix on copy). All quantities carry
/// their unit in the member name, so no Geant4 unit system leaks in here.

#ifndef ManifestData_h
#define ManifestData_h 1

#include <string>
#include <vector>

namespace ManifestData
{
  /// The beam as the primary generator had it set when a run started.
  struct Beam
  {
    std::string particle;
    double energy_keV = 0.;
    double position_um[3] = {0., 0., 0.};
    double direction[3] = {0., 0., 1.};
  };

  /// One /run/beamOn folded into a dump.
  struct RunRecord
  {
    int runId = 0;
    long events = 0;
    bool hasBeam = false;
    Beam beam;
    long seed = 0;
    double energyDeposit_eV = 0.;
  };

  /// One exogenous bulk scavenger (0 = absent is simply not listed).
  struct Scavenger
  {
    std::string species;
    double molarity_M = 0.;
  };

  /// Everything a dump's Manifest.json states. Totals (events, energy
  /// deposit) are not stored: ManifestWriter derives them from `runs`.
  struct Manifest
  {
    int schemaVersion = 1;
    std::string timestamp;
    std::string geant4Version;
    std::string macro;
    std::string chemistry;
    std::vector<Scavenger> scavengers;
    double pH = 7.;
    double chemistryEndTime_ns = 0.;
    std::string runMode;
    int threads = 1;
    std::string outputDirAsConfigured;
    std::string outputDirAbsolute;
    std::string prefix;
    std::string subdir;
    std::vector<std::string> files;
    std::vector<RunRecord> runs;
  };
} // namespace ManifestData

#endif // ManifestData_h
