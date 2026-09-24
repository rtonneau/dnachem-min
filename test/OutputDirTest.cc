/// \file OutputDirTest.cc
/// \brief Plain-assert unit tests for OutputDir (no test framework, no
/// Geant4 runtime -- exercises pure directory-configuration/path-join logic
/// only).

#include "OutputDir.hh"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace
{
  fs::path TestRoot()
  {
    return fs::temp_directory_path() / "OutputDirTest";
  }

  void ResetTestRoot()
  {
    std::error_code ec;
    fs::remove_all(TestRoot(), ec);
    fs::create_directories(TestRoot(), ec);
  }
}

// --- Configure ---------------------------------------------------------

static void TestConfigureEmptyDirIsNoOp()
{
  G4String err;
  assert(OutputDir::Configure("", err));
  assert(err.empty());
}

static void TestConfigureCreatesMissingLeafDirectory()
{
  ResetTestRoot();
  fs::path target = TestRoot() / "fresh";
  assert(!fs::exists(target));

  G4String err;
  assert(OutputDir::Configure(target.string().c_str(), err));
  assert(fs::exists(target));
  assert(fs::is_directory(target));
}

static void TestConfigureAcceptsAlreadyExistingDirectory()
{
  ResetTestRoot();
  fs::path target = TestRoot() / "already-there";
  fs::create_directory(target);

  G4String err;
  assert(OutputDir::Configure(target.string().c_str(), err));
}

static void TestConfigureRejectsPathThatIsAFile()
{
  ResetTestRoot();
  fs::path target = TestRoot() / "im-a-file";
  std::ofstream(target.string()) << "x";

  G4String err;
  assert(!OutputDir::Configure(target.string().c_str(), err));
  assert(!err.empty());
}

static void TestConfigureRejectsMissingParent()
{
  ResetTestRoot();
  fs::path target = TestRoot() / "no-such-parent" / "leaf";

  G4String err;
  assert(!OutputDir::Configure(target.string().c_str(), err));
  assert(!err.empty());
  assert(!fs::exists(target));
}

// --- ConfigureFromMacro --------------------------------------------------

static void TestConfigureFromMacroSetsDirWhenNoneConfiguredYet()
{
  ResetTestRoot();
  G4String err;
  assert(OutputDir::Configure("", err)); // simulates --dir absent

  fs::path target = TestRoot() / "macro-only";
  assert(OutputDir::ConfigureFromMacro(target.string().c_str(), err));
  assert(err.empty());
  assert(fs::exists(target));
  assert(fs::is_directory(target));
}

static void TestConfigureFromMacroNoOpWhenSamePathAlreadyConfigured()
{
  ResetTestRoot();
  fs::path target = TestRoot() / "same-path";

  G4String err;
  assert(OutputDir::Configure(target.string().c_str(), err)); // simulates --dir <target>
  assert(OutputDir::ConfigureFromMacro(target.string().c_str(), err));
  assert(err.empty());
}

static void TestConfigureFromMacroConflictsWithDifferentCliPath()
{
  ResetTestRoot();
  fs::path cliTarget = TestRoot() / "from-cli";
  fs::path macroTarget = TestRoot() / "from-macro";

  G4String err;
  assert(OutputDir::Configure(cliTarget.string().c_str(), err)); // simulates --dir <cliTarget>
  assert(!OutputDir::ConfigureFromMacro(macroTarget.string().c_str(), err));
  assert(!err.empty());

  // State must be unchanged: Resolve() still uses the CLI-configured directory.
  fs::path expected = cliTarget / "Species.Txt";
  assert(OutputDir::Resolve("Species.Txt") == expected.string().c_str());
}

static void TestConfigureFromMacroConflictsWithDifferentEarlierMacroPath()
{
  ResetTestRoot();
  G4String err;
  assert(OutputDir::Configure("", err)); // simulates --dir absent

  fs::path firstTarget = TestRoot() / "macro-first";
  fs::path secondTarget = TestRoot() / "macro-second";

  assert(OutputDir::ConfigureFromMacro(firstTarget.string().c_str(), err));
  assert(!OutputDir::ConfigureFromMacro(secondTarget.string().c_str(), err));
  assert(!err.empty());

  fs::path expected = firstTarget / "Species.Txt";
  assert(OutputDir::Resolve("Species.Txt") == expected.string().c_str());
}

// --- Resolve -------------------------------------------------------------

static void TestResolveWithoutConfigureReturnsFilenameUnchanged()
{
  G4String err;
  OutputDir::Configure("", err); // explicit empty configure = "not set"

  assert(OutputDir::Resolve("Species.Txt") == "Species.Txt");
}

static void TestResolveJoinsConfiguredDirectory()
{
  ResetTestRoot();
  fs::path target = TestRoot() / "joined";

  G4String err;
  assert(OutputDir::Configure(target.string().c_str(), err));

  fs::path expected = target / "Species.Txt";
  assert(OutputDir::Resolve("Species.Txt") == expected.string().c_str());
}

// --- SetPrefix / Resolve prefix -------------------------------------------

static void TestResolveWithoutPrefixLeavesFilenameUnchanged()
{
  G4String err;
  OutputDir::Configure("", err);
  OutputDir::SetPrefix("");

  assert(OutputDir::Resolve("Species.Txt") == "Species.Txt");
}

static void TestResolvePrependsPrefixToFilename()
{
  G4String err;
  OutputDir::Configure("", err);
  OutputDir::SetPrefix("run1_");

  assert(OutputDir::Resolve("Species.Txt") == "run1_Species.Txt");

  OutputDir::SetPrefix(""); // restore for later tests
}

static void TestResolveAppliesPrefixBeforeJoiningDirectory()
{
  ResetTestRoot();
  fs::path target = TestRoot() / "prefixed";

  G4String err;
  assert(OutputDir::Configure(target.string().c_str(), err));
  OutputDir::SetPrefix("run2_");

  fs::path expected = target / "run2_Species.Txt";
  assert(OutputDir::Resolve("Species.Txt") == expected.string().c_str());

  OutputDir::SetPrefix("");        // restore for later tests
  OutputDir::Configure("", err);   // restore for later tests
}

int main()
{
  TestConfigureEmptyDirIsNoOp();
  TestConfigureCreatesMissingLeafDirectory();
  TestConfigureAcceptsAlreadyExistingDirectory();
  TestConfigureRejectsPathThatIsAFile();
  TestConfigureRejectsMissingParent();
  TestConfigureFromMacroSetsDirWhenNoneConfiguredYet();
  TestConfigureFromMacroNoOpWhenSamePathAlreadyConfigured();
  TestConfigureFromMacroConflictsWithDifferentCliPath();
  TestConfigureFromMacroConflictsWithDifferentEarlierMacroPath();
  TestResolveWithoutConfigureReturnsFilenameUnchanged();
  TestResolveJoinsConfiguredDirectory();
  TestResolveWithoutPrefixLeavesFilenameUnchanged();
  TestResolvePrependsPrefixToFilename();
  TestResolveAppliesPrefixBeforeJoiningDirectory();

  std::error_code ec;
  fs::remove_all(TestRoot(), ec);

  std::cout << "All OutputDir tests passed." << std::endl;
  return 0;
}
