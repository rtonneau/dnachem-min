/// \file sim.cc
/// \brief Geant example implementing Voxelized geometry

#include "ActionInitialization.hh"
#include "DetectorConstruction.hh"
#include "PhysicsList.hh"

#include "G4ScoringManager.hh"
#include "G4DNAChemistryManager.hh"
#include "G4RunManagerFactory.hh"
#include "G4UIExecutive.hh"
#include "G4UImanager.hh"
#include "G4VisExecutive.hh"
#include "G4Timer.hh"

#include <time.h>

static const G4bool useGUI = false;
std::ofstream out;

int main(int argc, char **argv)
{

  // Instantiate the G4Timer object, to monitor the CPU time spent for
  // the entire execution
  G4Timer *theTimer = new G4Timer();
  theTimer->Start();

  // G4RunManagerType runManagerType = G4RunManagerType::Serial;
  G4RunManagerType runManagerType = G4RunManagerType::MT;

  G4RunManager *runManager = G4RunManagerFactory::CreateRunManager(runManagerType);
  G4UImanager *UIManager = G4UImanager::GetUIpointer();

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
    G4cout << "starting batch mode" << G4endl;
    // Batch mode execution
    // rem: Initialize is performed in beam.in macro!
    UIManager->ApplyCommand("/control/execute macro/beam.in");
  }

  // Stop the benchmark here
  theTimer->Stop();

  G4cout << "The simulation took: " << theTimer->GetRealElapsed() << " s to run (real time)"
         << G4endl;

  // Clean up
  delete theTimer;
  delete runManager;

  return 0;
}
