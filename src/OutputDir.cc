/// \file OutputDir.cc
/// \brief Implementation of OutputDir

#include "OutputDir.hh"

#include <filesystem>

namespace
{
  G4String gConfiguredDir = "";
  G4String gPrefix = "";
}

G4bool OutputDir::Configure(const G4String &dir, G4String &err)
{
  if (dir.empty())
  {
    gConfiguredDir = "";
    return true;
  }

  std::filesystem::path target(dir.c_str());
  std::error_code ec;

  if (std::filesystem::exists(target, ec))
  {
    if (!std::filesystem::is_directory(target, ec))
    {
      err = dir + ": already exists and is not a directory";
      return false;
    }
    gConfiguredDir = dir;
    return true;
  }

  if (!std::filesystem::create_directory(target, ec) || ec)
  {
    err = dir + ": could not create directory (does its parent exist?)";
    return false;
  }

  gConfiguredDir = dir;
  return true;
}

G4bool OutputDir::ConfigureFromMacro(const G4String &dir, G4String &err)
{
  if (!gConfiguredDir.empty())
  {
    if (dir == gConfiguredDir)
      return true;

    err = "output directory already set to '" + gConfiguredDir + "'; '" + dir
          + "' conflicts with it -- use only one of --dir or /run/outputDir, "
            "with matching values";
    return false;
  }

  return Configure(dir, err);
}

G4String OutputDir::Resolve(const G4String &filename)
{
  G4String name = gPrefix.empty() ? filename : G4String(gPrefix + filename);

  if (gConfiguredDir.empty())
    return name;

  std::filesystem::path joined = std::filesystem::path(gConfiguredDir.c_str()) / name.c_str();
  return G4String(joined.string().c_str());
}

void OutputDir::SetPrefix(const G4String &prefix)
{
  gPrefix = prefix;
}
