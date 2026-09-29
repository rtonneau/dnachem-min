/// \file RunManifest.cc
/// \brief Implementation of RunManifest

#include "scoring/RunManifest.hh"

#include "actions/Run.hh"
#include "chemistry/ChemistryRegistry.hh"
#include "core/OutputDir.hh"
#include "geometry/DetectorConstruction.hh"
#include "geometry/DnaChemistryWorld.hh"
#include "scoring/DataNode.hh"
#include "scoring/JsonWriter.hh"
#include "scoring/RunAccumulator.hh"

#include "G4Exception.hh"
#include "G4MTRunManager.hh"
#include "G4RunManager.hh"
#include "G4Scheduler.hh"
#include "G4SystemOfUnits.hh"
#include "G4Version.hh"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{
  G4String gMacroName;

  std::string Timestamp()
  {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream stream;
    stream << std::put_time(&local, "%Y-%m-%dT%H:%M:%S");
    return stream.str();
  }

  // G4VERSION_TAG looks like "$Name: geant4-11-04-patch-01 $".
  std::string Geant4Version()
  {
    std::string tag = G4VERSION_TAG;
    const std::string head = "$Name:";
    if (tag.compare(0, head.size(), head) == 0)
      tag.erase(0, head.size());
    while (!tag.empty() && (tag.back() == '$' || tag.back() == ' '))
      tag.pop_back();
    const std::string::size_type first = tag.find_first_not_of(' ');
    return first == std::string::npos ? std::string() : tag.substr(first);
  }

  DataNode Vec3(const double *values)
  {
    return DataNode::MakeArray().Push(values[0]).Push(values[1]).Push(values[2]);
  }
}

void RunManifest::SetMacroName(const G4String &macro)
{
  gMacroName = macro;
}

void RunManifest::RecordRun(const Run &run)
{
  DataNode entry = DataNode::MakeObject();
  entry.Add("run", run.GetRunID());
  entry.Add("events", static_cast<long>(run.GetNumberOfEvent()));
  if (run.HasBeam())
  {
    const Run::Beam &beam = run.GetBeam();
    entry.Add("particle", beam.particle);
    entry.Add("beamEnergy_keV", beam.energy_keV);
    entry.Add("position_um", Vec3(beam.position_um));
    entry.Add("direction", Vec3(beam.direction));
  }
  else
  {
    entry.Add("particle", DataNode());
    entry.Add("beamEnergy_keV", DataNode());
    entry.Add("position_um", DataNode());
    entry.Add("direction", DataNode());
  }
  entry.Add("energyDeposit_eV", run.GetSumDose() / eV);
  entry.Add("seed", run.GetSeed());
  RunAccumulator::AddRunEntry(entry);
}

void RunManifest::Write(const G4String &prefix, const G4String &subdir,
                        const std::vector<std::string> &files)
{
  G4RunManager *runManager = G4RunManager::GetRunManager();
  auto *mtRunManager = dynamic_cast<G4MTRunManager *>(runManager);

  const ChemistryRegistry::Chemistry *chemistry = ChemistryRegistry::Selected();

  // Environment set on the chemistry world (pH, scavengers), if the detector
  // has one.
  const auto *detector =
      dynamic_cast<const DetectorConstruction *>(runManager->GetUserDetectorConstruction());
  const auto *chemistryWorld =
      (detector != nullptr) ? dynamic_cast<const DnaChemistryWorld *>(detector->GetChemistryWorld())
                            : nullptr;
  DataNode scavengers = DataNode::MakeArray();
  if (chemistryWorld != nullptr)
  {
    for (const ScavengerSpec::Entry &entry : chemistryWorld->GetScavengers())
      scavengers.Push(DataNode::MakeObject()
                          .Add("species", std::string(entry.species))
                          .Add("molarity_M", entry.molarity));
  }

  const std::string dir = OutputDir::GetDirectory();
  std::error_code ec;
  const std::filesystem::path absolute =
      std::filesystem::absolute(std::filesystem::path(dir.empty() ? "." : dir), ec);

  DataNode fileList = DataNode::MakeArray();
  for (const std::string &file : files)
    fileList.Push(file);

  DataNode runs = DataNode::MakeArray();
  for (const DataNode &entry : RunAccumulator::GetRunEntries())
    runs.Push(entry);

  // The manifest's entries, in output order. Add or remove a line here to
  // change what Manifest.json states; JsonWriter needs no change.
  DataNode manifest = DataNode::MakeObject();
  manifest.Add("schemaVersion", 1);
  manifest.Add("timestamp", Timestamp());
  manifest.Add("geant4Version", Geant4Version());
  manifest.Add("macro", std::string(gMacroName));
  manifest.Add("chemistry", (chemistry != nullptr) ? std::string(chemistry->name) : std::string());
  manifest.Add("scavengers", scavengers);
  manifest.Add("pH", (chemistryWorld != nullptr) ? chemistryWorld->GetpH() : 7.);
  manifest.Add("halfBox_um",
               (chemistryWorld != nullptr) ? DataNode(chemistryWorld->GetHalfBox() / um) : DataNode());
  manifest.Add("chemistryEndTime_ns", G4Scheduler::Instance()->GetEndTime() / ns);
  manifest.Add("runMode", (mtRunManager != nullptr) ? "MT" : "Serial");
  manifest.Add("threads", (mtRunManager != nullptr) ? mtRunManager->GetNumberOfThreads() : 1);
  manifest.Add("outputDirAsConfigured", dir);
  manifest.Add("outputDirAbsolute", ec ? dir : absolute.lexically_normal().string());
  manifest.Add("prefix", std::string(prefix));
  manifest.Add("subdir", std::string(subdir));
  manifest.Add("totalEvents", RunAccumulator::GetAccumulatedEvents());
  manifest.Add("totalEnergyDeposit_eV", RunAccumulator::GetAccumulatedEnergy() / eV);
  manifest.Add("files", fileList);
  manifest.Add("runs", runs);

  const G4String path = OutputDir::Resolve("Manifest.json");
  std::ofstream out(path);
  if (!out)
  {
    G4Exception("RunManifest::Write", "ManifestWriteFailed", JustWarning,
                ("could not open '" + path + "' for writing; the dump's data files are "
                 "written but it has no manifest")
                    .c_str());
    return;
  }
  JsonWriter::Write(out, manifest);
}
