/// \file RunManifest.cc
/// \brief Implementation of RunManifest

#include "scoring/RunManifest.hh"

#include "chemistry/ChemistryRegistry.hh"
#include "core/OutputDir.hh"
#include "geometry/DetectorConstruction.hh"
#include "geometry/DnaChemistryWorld.hh"
#include "scoring/ManifestData.hh"
#include "scoring/ManifestWriter.hh"
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
}

void RunManifest::SetMacroName(const G4String &macro)
{
  gMacroName = macro;
}

void RunManifest::Write(const G4String &prefix, const G4String &subdir,
                        const std::vector<std::string> &files)
{
  ManifestData::Manifest manifest;
  manifest.timestamp = Timestamp();
  manifest.geant4Version = Geant4Version();
  manifest.macro = gMacroName;

  const ChemistryRegistry::Chemistry *chemistry = ChemistryRegistry::Selected();
  if (chemistry != nullptr)
    manifest.chemistry = chemistry->name;

  G4RunManager *runManager = G4RunManager::GetRunManager();
  auto *mtRunManager = dynamic_cast<G4MTRunManager *>(runManager);
  manifest.runMode = (mtRunManager != nullptr) ? "MT" : "Serial";
  manifest.threads = (mtRunManager != nullptr) ? mtRunManager->GetNumberOfThreads() : 1;

  manifest.chemistryEndTime_ns = G4Scheduler::Instance()->GetEndTime() / ns;

  // Environment set on the chemistry world (pH, scavengers), if the detector
  // has one.
  const auto *detector =
      dynamic_cast<const DetectorConstruction *>(runManager->GetUserDetectorConstruction());
  const auto *chemistryWorld =
      (detector != nullptr) ? dynamic_cast<const DnaChemistryWorld *>(detector->GetChemistryWorld())
                            : nullptr;
  if (chemistryWorld != nullptr)
  {
    manifest.pH = chemistryWorld->GetpH();
    for (const ScavengerSpec::Entry &entry : chemistryWorld->GetScavengers())
    {
      ManifestData::Scavenger scavenger;
      scavenger.species = entry.species;
      scavenger.molarity_M = entry.molarity;
      manifest.scavengers.push_back(scavenger);
    }
  }

  const std::string dir = OutputDir::GetDirectory();
  manifest.outputDirAsConfigured = dir;
  std::error_code ec;
  const std::filesystem::path absolute =
      std::filesystem::absolute(std::filesystem::path(dir.empty() ? "." : dir), ec);
  manifest.outputDirAbsolute = ec ? dir : absolute.lexically_normal().string();

  manifest.prefix = prefix;
  manifest.subdir = subdir;
  manifest.files = files;
  manifest.runs = RunAccumulator::GetRunRecords();

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
  ManifestWriter::Write(out, manifest);
}
