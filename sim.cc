/// \file sim.cc
/// \brief Basic common implementation of water radiolysis for CONV and UHDR
// Created: 2026-08-19

#include "ActionInitialization.hh"
#include "DetectorConstruction.hh"
#include "DnaLogger.hh"
#include "DnaLoggerMessenger.hh"
#include "PhysicsList.hh"

#include "G4ScoringManager.hh"
#include "G4DNAChemistryManager.hh"
#include "G4RunManagerFactory.hh"
#include "G4Threading.hh"
#include "G4UIExecutive.hh"
#include "G4UImanager.hh"
#include "G4VisExecutive.hh"
#include "G4Timer.hh"
#include "Randomize.hh"

#include <cstdlib>
#include <time.h>

static const G4bool useGUI = false;
std::ofstream out;

// Parses an optional "--threads N" flag out of argv (the macro file, if given,
// must be argv[1] and precede it). Returns the validated thread count, or 0 if
// the flag was not given. Prints via DnaLogger and exits(1) on any malformed
// or out-of-range value -- the DnaLogger level is forced to Error first since
// it otherwise defaults to Quiet before "/dnaLogger/verbose" can be applied.
static G4int ParseThreadsArg(int argc, char **argv)
{
  for (int i = 1; i < argc; ++i)
  {
    if (G4String(argv[i]) != "--threads")
      continue;

    auto fail = [](const G4String &message)
    {
      DnaLogger::SetLevel(DnaLogger::Level::Error);
      DnaLogger::Print(DnaLogger::Level::Error, "--threads: " + message);
      exit(1);
    };

    if (i + 1 >= argc)
      fail("missing value");

    const char *value = argv[i + 1];
    char *end = nullptr;
    long parsed = std::strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed <= 0)
      fail("value must be a positive integer, got '" + G4String(value) + "'");

    G4int available = G4Threading::G4GetNumberOfCores();
    if (parsed > available)
      fail("requested " + std::to_string(parsed) + " threads, but only "
           + std::to_string(available) + " cores are available");

    return static_cast<G4int>(parsed);
  }

  return 0;
}

// Makes the RNG seed an explicit, documented property instead of an
// undocumented CLHEP default. This reproduces the pre-chemistry physics
// stage exactly (verified: identical track lists/counts across separate
// process launches) but does NOT make the full pipeline reproducible --
// the chemistry (IT) stepping stage still diverges run-to-run even with
// this seed fixed; see .scratch/geant4-testing/spec.md addendum and
// ticket 02 for the investigation. A macro's "/random/setSeeds <a> <b>"
// overrides this before "/run/initialize" runs.
constexpr long kDefaultSeed = 12345;

int main(int argc, char **argv)
{
  DnaLogger::SetLevel(DnaLogger::Level::Quiet);

  G4Random::setTheSeed(kDefaultSeed);

  // Instantiate the G4Timer object, to monitor the CPU time spent for
  // the entire execution
  G4Timer *theTimer = new G4Timer();
  theTimer->Start();

  // Default is Serial (single-threaded, reproducible). Pass "--threads N" on
  // the command line to run multithreaded with N worker threads instead.
  G4int requestedThreads = ParseThreadsArg(argc, argv);
  G4RunManagerType runManagerType =
      (requestedThreads > 0) ? G4RunManagerType::MT : G4RunManagerType::Serial;

  G4RunManager *runManager = G4RunManagerFactory::CreateRunManager(runManagerType);

  // A macro may still override with "/run/numberOfThreads N" before
  // "/run/initialize".
  if (!useGUI && runManagerType != G4RunManagerType::Serial)
    runManager->SetNumberOfThreads(requestedThreads);

  G4UImanager *UIManager = G4UImanager::GetUIpointer();

  // Exposes "/dnaLogger/verbose <level>" so the logging level can be set from a macro
  DnaLoggerMessenger *dnaLoggerMessenger = new DnaLoggerMessenger();

  //////////
  // Set mandatory user initialization classes
  //
  DetectorConstruction *detector = new DetectorConstruction();
  runManager->SetUserInitialization(detector);
  runManager->SetUserInitialization(new PhysicsList);
  runManager->SetUserInitialization(new ActionInitialization());

  G4String storage = ".";

  // toml->SetRootPath(storage);
  // Start Simulations
  if (useGUI)
  {
    // ----------------------
    // - GUI mode execution
    // ----------------------
    runManager->SetNumberOfThreads(1);
    runManager->Initialize();
    G4cout << "starting GUI" << G4endl;
    // Initialize visualization
    G4VisManager *visManager = new G4VisExecutive;
    visManager->Initialize();

    // Start interactive session
    G4UIExecutive *ui = new G4UIExecutive(argc, argv);
    // UIManager->ApplyCommand("/vis/open OGL");
    UIManager->ApplyCommand("/control/execute macro/vis.mac");
    ui->SessionStart();

    // Clean up GUI resources
    delete ui;
    delete visManager;
  }
  else
  {
    // ------------------------
    // - Batch mode execution
    // ------------------------
    G4String macroFile = "macro/" + ((argc > 1) ? G4String(argv[1]) : G4String("beam.in"));
    G4cout << "starting batch mode with macro file: " << macroFile << G4endl;
    // Batch mode execution
    // rem: Initialize is performed in beam.in macro!

    UIManager->ApplyCommand("/control/execute " + macroFile);
  }

  // Stop the benchmark here
  theTimer->Stop();

  G4cout << "The simulation took: " << theTimer->GetRealElapsed() << " s to run (real time)"
         << G4endl;

  // Clean up
  delete theTimer;
  delete dnaLoggerMessenger;
  delete runManager;

  return 0;
}
