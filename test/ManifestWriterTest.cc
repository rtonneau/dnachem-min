/// \file ManifestWriterTest.cc
/// \brief Plain-assert unit tests for ManifestWriter (no test framework, no
/// Geant4 runtime -- exercises the pure JSON serialisation only).

#include "scoring/ManifestWriter.hh"

#include <cassert>
#include <iostream>
#include <limits>
#include <sstream>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

static std::string Render(const ManifestData::Manifest &manifest)
{
  std::ostringstream stream;
  ManifestWriter::Write(stream, manifest);
  return stream.str();
}

static void TestEscapeJson()
{
  assert(ManifestWriter::EscapeJson("a\"b\\c\nd\te") == "a\\\"b\\\\c\\nd\\te");
  assert(ManifestWriter::EscapeJson(std::string(1, '\x01')) == "\\u0001");
}

static void TestTotalsAreSummedFromRuns()
{
  ManifestData::Manifest manifest;
  ManifestData::RunRecord a;
  a.runId = 0;
  a.events = 2;
  a.energyDeposit_eV = 100.;
  ManifestData::RunRecord b;
  b.runId = 1;
  b.events = 3;
  b.energyDeposit_eV = 50.5;
  manifest.runs = {a, b};

  const std::string out = Render(manifest);
  assert(out.find("\"totalEvents\": 5,") != std::string::npos);
  assert(out.find("\"totalEnergyDeposit_eV\": 150.5,") != std::string::npos);
}

static void TestRunLineWithBeam()
{
  ManifestData::Manifest manifest;
  ManifestData::RunRecord run;
  run.runId = 0;
  run.events = 2;
  run.hasBeam = true;
  run.seed = 12345;
  run.energyDeposit_eV = 123.5;
  run.beam.particle = "e-";
  run.beam.energy_keV = 10.;
  manifest.runs = {run};

  assert(Render(manifest).find("{\"run\": 0, \"events\": 2, \"particle\": \"e-\", "
                               "\"beamEnergy_keV\": 10, \"position_um\": [0, 0, 0], "
                               "\"direction\": [0, 0, 1], \"energyDeposit_eV\": 123.5, "
                               "\"seed\": 12345}") != std::string::npos);
}

static void TestRunWithoutBeamPrintsNull()
{
  ManifestData::Manifest manifest;
  ManifestData::RunRecord run;
  run.runId = 4;
  run.events = 1;
  manifest.runs = {run};

  assert(Render(manifest).find("\"particle\": null, \"beamEnergy_keV\": null, "
                               "\"position_um\": null, \"direction\": null") !=
         std::string::npos);
}

static void TestEmptyArraysAndEscapedStrings()
{
  ManifestData::Manifest manifest;
  manifest.macro = "C:\\macro\\a\"b.in";

  const std::string out = Render(manifest);
  assert(out.find("\"runs\": []") != std::string::npos);
  assert(out.find("\"files\": []") != std::string::npos);
  assert(out.find("\"scavengers\": []") != std::string::npos);
  assert(out.find("\"macro\": \"C:\\\\macro\\\\a\\\"b.in\"") != std::string::npos);
  assert(out.find("\"schemaVersion\": 1,") != std::string::npos);
}

static void TestScavengerAndFileLines()
{
  ManifestData::Manifest manifest;
  manifest.scavengers = {{"O2", 0.000273}};
  manifest.files = {"Species.Txt", "run1_Reactions.Txt"};

  const std::string out = Render(manifest);
  assert(out.find("{\"species\": \"O2\", \"molarity_M\": 0.000273}") != std::string::npos);
  assert(out.find("\"Species.Txt\"") != std::string::npos);
  assert(out.find("\"run1_Reactions.Txt\"") != std::string::npos);
}

static void TestNonFiniteNumberPrintsNull()
{
  ManifestData::Manifest manifest;
  manifest.pH = std::numeric_limits<double>::quiet_NaN();
  assert(Render(manifest).find("\"pH\": null,") != std::string::npos);
}

static void TestKeyOrder()
{
  const std::string out = Render(ManifestData::Manifest());
  const char *keys[] = {"schemaVersion",     "timestamp",      "geant4Version",
                        "macro",             "chemistry",      "scavengers",
                        "pH",                "chemistryEndTime_ns", "runMode",
                        "threads",           "outputDirAsConfigured", "outputDirAbsolute",
                        "prefix",            "subdir",         "totalEvents",
                        "totalEnergyDeposit_eV", "files",      "runs"};
  std::string::size_type last = 0;
  for (const char *key : keys)
  {
    const std::string::size_type pos = out.find(std::string("\"") + key + "\":");
    assert(pos != std::string::npos);
    assert(pos >= last);
    last = pos;
  }
}

int main()
{
#ifdef _MSC_VER
  // Route Debug-CRT assert failures to stderr instead of a blocking dialog,
  // so a failing assert() exits the process instead of hanging CI/ctest.
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif

  TestEscapeJson();
  TestTotalsAreSummedFromRuns();
  TestRunLineWithBeam();
  TestRunWithoutBeamPrintsNull();
  TestEmptyArraysAndEscapedStrings();
  TestScavengerAndFileLines();
  TestNonFiniteNumberPrintsNull();
  TestKeyOrder();

  std::cout << "ManifestWriterTest: all tests passed" << std::endl;
  return 0;
}
