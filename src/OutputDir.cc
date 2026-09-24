/// \file OutputDir.cc
/// \brief Implementation of OutputDir

#include "OutputDir.hh"

#include <filesystem>

namespace
{
  G4String gConfiguredDir = "";
  G4String gPrefix = "";
  G4String gSubdir = "";
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

  if (gConfiguredDir.empty() && gSubdir.empty())
    return name;

  std::filesystem::path joined(gConfiguredDir.c_str());
  if (!gSubdir.empty())
    joined /= gSubdir.c_str();
  joined /= name.c_str();
  return G4String(joined.string().c_str());
}

void OutputDir::SetPrefix(const G4String &prefix)
{
  gPrefix = prefix;
}

G4bool OutputDir::ConfigureSubdir(const G4String &subdir, G4String &err)
{
  if (subdir.empty())
  {
    gSubdir = "";
    return true;
  }

  std::filesystem::path sub(subdir.c_str());
  if (sub.has_root_path())
  {
    err = subdir + ": must be a relative path";
    return false;
  }
  for (const auto &part : sub)
  {
    if (part == "..")
    {
      err = subdir + ": must not contain a '..' component";
      return false;
    }
  }

  std::filesystem::path target(gConfiguredDir.c_str());
  target /= sub;

  std::error_code ec;
  if (std::filesystem::exists(target, ec))
  {
    if (!std::filesystem::is_directory(target, ec))
    {
      err = subdir + ": already exists and is not a directory";
      return false;
    }
  }
  else if (!std::filesystem::create_directories(target, ec) || ec)
  {
    err = subdir + ": could not create directory";
    return false;
  }

  gSubdir = subdir;
  return true;
}
