/// \file SpeciesSampleTimesMessenger.cc
/// \brief Implementation of the species sample-time commands (see the header).

#include "scoring/SpeciesSampleTimesMessenger.hh"

#include "G4ApplicationState.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithAnInteger.hh"
#include "G4UIdirectory.hh"

#include <string>

SpeciesSampleTimes::Setting SpeciesSampleTimesMessenger::fSetting;
std::atomic<std::size_t> SpeciesSampleTimesMessenger::fLastGridSize{0};

namespace
{
  const char* ModeCommand(SpeciesSampleTimes::Mode mode)
  {
    switch (mode) {
      case SpeciesSampleTimes::Mode::Fixed:
        return "timesFixed";
      case SpeciesSampleTimes::Mode::List:
        return "timesList";
      case SpeciesSampleTimes::Mode::PerDecade:
        return "timesPerDecade";
      case SpeciesSampleTimes::Mode::Default:
        break;
    }
    return "default";
  }
}  // namespace

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

SpeciesSampleTimesMessenger::SpeciesSampleTimesMessenger()
{
  // Not broadcast: the setting is process-wide and set once on the master.
  fpDirectory = new G4UIdirectory("/scoring/species/", false);
  fpDirectory->SetGuidance(
    "Times at which the species are scored. Use only one of timesFixed, timesList, "
    "timesPerDecade per process; none -> 1 ps, 10 ps, ..., 1 us, then the chemistry end time.");

  fpFixedCmd = new G4UIcmdWithADoubleAndUnit("/scoring/species/timesFixed", this);
  fpFixedCmd->SetGuidance(
    "Score the species every <step>: step, 2*step, ... below the chemistry end time, then "
    "the end time. Must be issued before /run/initialize.");
  fpFixedCmd->SetParameterName("step", false);
  fpFixedCmd->SetUnitCategory("Time");
  fpFixedCmd->AvailableForStates(G4State_PreInit);
  fpFixedCmd->SetToBeBroadcasted(false);

  fpListCmd = new G4UIcmdWithAString("/scoring/species/timesList", this);
  fpListCmd->SetGuidance(
    "Score the species at these times: '<t1> ... <tN> <unit>' (e.g. '1 10 100 nanosecond'). "
    "Times above the chemistry end time are dropped with a warning. Must be issued before "
    "/run/initialize.");
  fpListCmd->SetParameterName("times", false);
  fpListCmd->AvailableForStates(G4State_PreInit);
  fpListCmd->SetToBeBroadcasted(false);

  fpPerDecadeCmd = new G4UIcmdWithAnInteger("/scoring/species/timesPerDecade", this);
  fpPerDecadeCmd->SetGuidance(
    "Score the species at N log-spaced times per decade from 1 ps (1 ps * 10^(k/N)) below "
    "the chemistry end time, then the end time. Must be issued before /run/initialize.");
  fpPerDecadeCmd->SetParameterName("N", false);
  fpPerDecadeCmd->AvailableForStates(G4State_PreInit);
  fpPerDecadeCmd->SetToBeBroadcasted(false);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

SpeciesSampleTimesMessenger::~SpeciesSampleTimesMessenger()
{
  delete fpFixedCmd;
  delete fpListCmd;
  delete fpPerDecadeCmd;
  delete fpDirectory;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void SpeciesSampleTimesMessenger::SetNewValue(G4UIcommand* command, G4String newValue)
{
  SpeciesSampleTimes::Setting candidate;

  if (command == fpFixedCmd) {
    candidate.mode = SpeciesSampleTimes::Mode::Fixed;
    candidate.step = fpFixedCmd->GetNewDoubleValue(newValue);  // internal units, ns = 1
  }
  else if (command == fpPerDecadeCmd) {
    candidate.mode = SpeciesSampleTimes::Mode::PerDecade;
    candidate.perDecade = fpPerDecadeCmd->GetNewIntValue(newValue);
  }
  else if (command == fpListCmd) {
    candidate.mode = SpeciesSampleTimes::Mode::List;
    std::string err;
    if (!SpeciesSampleTimes::ParseList(newValue, candidate.list, err)) {
      const std::string msg = "/scoring/species/timesList: " + err;
      G4Exception("SpeciesSampleTimesMessenger::SetNewValue", "InvalidSpeciesTimes",
                  FatalException, msg.c_str());
      return;
    }
  }
  else {
    return;
  }

  Apply(candidate);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void SpeciesSampleTimesMessenger::Apply(const SpeciesSampleTimes::Setting& candidate)
{
  if (fSetting.mode != SpeciesSampleTimes::Mode::Default && fSetting.mode != candidate.mode) {
    const std::string msg = std::string("/scoring/species/") + ModeCommand(candidate.mode)
                            + " conflicts with the earlier /scoring/species/"
                            + ModeCommand(fSetting.mode) + " -- use only one per process.";
    G4Exception("SpeciesSampleTimesMessenger::SetNewValue", "ConflictingSpeciesTimes",
                FatalException, msg.c_str());
    return;
  }

  std::string err;
  if (!SpeciesSampleTimes::Validate(candidate, err)) {
    const std::string msg =
      std::string("/scoring/species/") + ModeCommand(candidate.mode) + ": " + err;
    G4Exception("SpeciesSampleTimesMessenger::SetNewValue", "InvalidSpeciesTimes",
                FatalException, msg.c_str());
    return;
  }

  fSetting = candidate;  // same mode repeated: last value wins
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4String SpeciesSampleTimesMessenger::GetCurrentValue(G4UIcommand* /*command*/)
{
  return "";
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

const SpeciesSampleTimes::Setting& SpeciesSampleTimesMessenger::Current()
{
  return fSetting;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

std::size_t SpeciesSampleTimesMessenger::LastGridSize()
{
  return fLastGridSize.load();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void SpeciesSampleTimesMessenger::SetLastGridSize(std::size_t size)
{
  fLastGridSize.store(size);
}
