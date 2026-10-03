/// \file SpeciesSampleTimesMessenger.hh
/// \brief Macro commands choosing the times at which species are scored.
///   /scoring/species/timesFixed <value> <unit>        k * step, then the end time
///   /scoring/species/timesList <t1> ... <tN> <unit>   the given times (<= end time)
///   /scoring/species/timesPerDecade <N>               1 ps * 10^(k/N), then the end time
/// None issued -> Default (decade points 1 ps ... 1 us, then the end time).
///
/// All three are PreInit only and are not broadcast to worker threads: the
/// setting is process-wide static state, written on the master before any
/// worker starts, and read by every thread through Current(). The grid
/// itself is built at run start (RunAction::BeginOfRunAction), because it
/// depends on the chemistry end time, which /scheduler/endTime may still
/// change after /run/initialize.
///
/// A plain G4UImessenger, not a G4GenericMessenger: DeclareMethod truncates
/// a multi-token string argument such as the timesList one.
#ifndef SpeciesSampleTimesMessenger_h
#define SpeciesSampleTimesMessenger_h 1

#include "scoring/SpeciesSampleTimes.hh"

#include "G4UImessenger.hh"

#include <atomic>
#include <cstddef>

class G4UIdirectory;
class G4UIcmdWithADoubleAndUnit;
class G4UIcmdWithAString;
class G4UIcmdWithAnInteger;

class SpeciesSampleTimesMessenger : public G4UImessenger
{
public:
  SpeciesSampleTimesMessenger();
  ~SpeciesSampleTimesMessenger() override;

  void SetNewValue(G4UIcommand* command, G4String newValue) override;
  G4String GetCurrentValue(G4UIcommand* command) override;

  /// The process-wide sample-time setting (Default until a command sets it).
  static const SpeciesSampleTimes::Setting& Current();

  /// Size of the last grid applied to a scorer (0 before the first run).
  static std::size_t LastGridSize();
  static void SetLastGridSize(std::size_t size);

private:
  /// Stores `candidate` as the process-wide setting, or raises the fatal
  /// G4Exception (conflicting mode, invalid value) at the command.
  static void Apply(const SpeciesSampleTimes::Setting& candidate);

  G4UIdirectory* fpDirectory;
  G4UIcmdWithADoubleAndUnit* fpFixedCmd;
  G4UIcmdWithAString* fpListCmd;
  G4UIcmdWithAnInteger* fpPerDecadeCmd;

  static SpeciesSampleTimes::Setting fSetting;
  static std::atomic<std::size_t> fLastGridSize;
};

#endif  // SpeciesSampleTimesMessenger_h
