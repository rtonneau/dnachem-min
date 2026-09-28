/// \file ChemistryRegistryTest.cc
/// \brief Plain-assert unit tests for ChemistryRegistry (no Geant4 runtime).
#include "ChemistryRegistry.hh"

#include <cassert>
#include <iostream>
#include <string>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace
{
void DummyReactions(G4DNAMolecularReactionTable*) {}
ChemistryTypes::AcidBaseList DummyAcidBase() { return {}; }

ChemistryRegistry::Chemistry Make(const std::string& name)
{
  return {name, &DummyReactions, &DummyAcidBase};
}

void MustRegister(const std::string& name)
{
  std::string err;
  const bool ok = ChemistryRegistry::Register(Make(name), err);
  assert(ok);
  (void)ok;
}

void ResetWithTwo()
{
  ChemistryRegistry::ResetForTesting();
  MustRegister("PureWater");
  MustRegister("BoscoloChem");
}

bool Contains(const std::string& text, const std::string& part)
{
  return text.find(part) != std::string::npos;
}
}  // namespace

static void TestRegisterRejectsBadInput()
{
  ChemistryRegistry::ResetForTesting();
  std::string err;
  assert(!ChemistryRegistry::Register(Make(""), err));
  assert(!err.empty());
  err.clear();
  assert(!ChemistryRegistry::Register({"NoBuilders", nullptr, nullptr}, err));
  assert(Contains(err, "null builder"));
  MustRegister("PureWater");
  err.clear();
  assert(!ChemistryRegistry::Register(Make("purewater"), err));
  assert(Contains(err, "already registered"));
}

static void TestNamesKeepRegistrationOrder()
{
  ResetWithTwo();
  const auto names = ChemistryRegistry::Names();
  assert(names.size() == 2);
  assert(names[0] == "PureWater");
  assert(names[1] == "BoscoloChem");
}

static void TestSelectedFallsBackToDefault()
{
  ChemistryRegistry::ResetForTesting();
  assert(ChemistryRegistry::Selected() == nullptr);
  ResetWithTwo();
  assert(ChemistryRegistry::Selected() != nullptr);
  assert(ChemistryRegistry::Selected()->name == ChemistryRegistry::kDefaultName);
}

static void TestSelectIsCaseInsensitiveAndKeepsCanonicalName()
{
  ResetWithTwo();
  std::string err;
  assert(ChemistryRegistry::Select("boscolochem", err));
  assert(ChemistryRegistry::Selected()->name == "BoscoloChem");
}

static void TestSelectUnknownListsValidNames()
{
  ResetWithTwo();
  std::string err;
  assert(!ChemistryRegistry::Select("Nope", err));
  assert(Contains(err, "Unknown chemistry 'Nope'"));
  assert(Contains(err, "PureWater"));
  assert(Contains(err, "BoscoloChem"));
  assert(ChemistryRegistry::Selected()->name == "PureWater");  // unchanged
}

static void TestSelectSameNameTwiceIsNoOp()
{
  ResetWithTwo();
  std::string err;
  assert(ChemistryRegistry::Select("BoscoloChem", err));
  assert(ChemistryRegistry::Select("BOSCOLOCHEM", err));
  assert(ChemistryRegistry::Selected()->name == "BoscoloChem");
}

static void TestSelectDifferentNameConflicts()
{
  ResetWithTwo();
  std::string err;
  assert(ChemistryRegistry::Select("PureWater", err));  // explicit default still counts
  assert(!ChemistryRegistry::Select("BoscoloChem", err));
  assert(Contains(err, "already set to 'PureWater'"));
  assert(ChemistryRegistry::Selected()->name == "PureWater");
}

static void TestResetClearsSelectionAndEntries()
{
  ResetWithTwo();
  std::string err;
  assert(ChemistryRegistry::Select("BoscoloChem", err));
  ChemistryRegistry::ResetForTesting();
  assert(ChemistryRegistry::Names().empty());
  assert(ChemistryRegistry::Selected() == nullptr);
}

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
  TestRegisterRejectsBadInput();
  TestNamesKeepRegistrationOrder();
  TestSelectedFallsBackToDefault();
  TestSelectIsCaseInsensitiveAndKeepsCanonicalName();
  TestSelectUnknownListsValidNames();
  TestSelectSameNameTwiceIsNoOp();
  TestSelectDifferentNameConflicts();
  TestResetClearsSelectionAndEntries();
  std::cout << "ChemistryRegistryTest: all tests passed\n";
  return 0;
}
