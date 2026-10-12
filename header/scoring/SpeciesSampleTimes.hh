/// \file SpeciesSampleTimes.hh
/// \brief Sample-time grids for species scoring (default / fixed step / list /
/// log-spaced per decade), plus the matching time-precision map.
///
/// Portable and project-agnostic: standard library only, no Geant4/CLHEP and no
/// dependency on other dnachem-min classes. Copy-paste portable to another
/// Geant4-DNA project (adjust the include path prefix on copy).
///
/// Every time in this file is in Geant4 internal units: nanosecond = 1, so
/// 1 ps = 1e-3 and 1 us = 1e3. The parsing and grid code never needs the
/// Geant4 unit tables.

#ifndef SpeciesSampleTimes_hh
#define SpeciesSampleTimes_hh 1

#include <map>
#include <string>
#include <vector>

namespace SpeciesSampleTimes
{
  /// How the sample times are chosen.
  ///  Default   decade points 1 ps ... 1 us, then the end time
  ///  Fixed     k * step, then the end time
  ///  List      the user-given times (those <= end time)
  ///  PerDecade 1 ps * 10^(k/N), then the end time
  enum class Mode
  {
    Default,
    Fixed,
    List,
    PerDecade
  };

  struct Setting
  {
    Mode mode = Mode::Default;
    double step = 0.;  ///< Fixed: step, ns
    int perDecade = 0; ///< PerDecade: points per decade (N)
    std::vector<double> list; ///< List: times, ns
  };

  /// Parses "v1 ... vN unit" (unit: picosecond/ps, nanosecond/ns,
  /// microsecond/us, millisecond/ms, second/s) into times in ns, sorted with
  /// duplicates removed. On failure returns false, sets `error` and leaves
  /// `timesOut` empty. Rejects: empty text, fewer than 2 tokens, an unknown
  /// unit, an unparsable value, a value <= 0.
  bool ParseList(const std::string& text, std::vector<double>& timesOut,
                 std::string& error);

  /// Sample times (ns), sorted and unique, for the given end time (ns).
  /// Default/Fixed/PerDecade always end with `endTime`; List keeps its values
  /// <= endTime and reports the ones > endTime in `droppedOut` (when given).
  std::vector<double> BuildGrid(const Setting& s, double endTime,
                                std::vector<double>* droppedOut = nullptr);

  /// Time-precision map (ns), independent of the end time: for a time t, the
  /// precision is the value of the first entry whose key is > t (upper_bound,
  /// as G4MoleculeCounterTimeComparer looks it up). Every value
  /// is <= 1e-2 (10 ps); the last key is DBL_MAX.
  std::map<double, double> PrecisionMap(const Setting& s);

  /// False (and sets `error`) for Fixed with step <= 0, PerDecade with N <= 0,
  /// List with no times.
  bool Validate(const Setting& s, std::string& error);
} // namespace SpeciesSampleTimes

#endif
