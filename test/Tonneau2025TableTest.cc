/// \file Tonneau2025TableTest.cc
/// \brief Plain-assert unit tests for Tonneau2025Table (no Geant4 runtime).
#include "chemistry/catalog/Tonneau2025Table.hh"

#include <cassert>
#include <cmath>
#include <iostream>
#include <set>
#include <string>
#include <vector>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

using namespace Tonneau2025Table;

namespace
{
bool Near(double a, double b) { return std::fabs(a - b) <= 1e-12 * std::fabs(b); }

bool Is(const std::vector<Term>& terms, const std::vector<Term>& expected)
{
  if (terms.size() != expected.size()) {
    return false;
  }
  for (std::size_t i = 0; i < terms.size(); ++i) {
    if (terms[i].species != expected[i].species || terms[i].count != expected[i].count) {
      return false;
    }
  }
  return true;
}
}  // namespace

static void TestCountAndIds()
{
  const auto& table = Reactions();
  assert(table.size() == 73);
  std::set<int> ids;
  for (std::size_t i = 0; i < table.size(); ++i) {
    assert(table[i].id == static_cast<int>(i) + 1);  // table order, R1..R73
    ids.insert(table[i].id);
    assert(Find(table[i].id) == &table[i]);
  }
  assert(ids.size() == 73);
  assert(Find(0) == nullptr);
  assert(Find(74) == nullptr);
}

static void TestEveryRowIsWellFormed()
{
  for (const auto& r : Reactions()) {
    assert(!r.reactants.empty());
    assert(!r.products.empty());
    assert(r.reactants.size() <= 3);
    assert(r.k > 0.0);
    assert(r.source >= 'A' && r.source <= 'D');
    for (const auto& t : r.reactants) {
      assert(!t.species.empty() && t.count >= 1);
    }
    for (const auto& t : r.products) {
      assert(!t.species.empty() && t.count >= 1);
    }
  }
}

static void TestPinnedRates()
{
  assert(Near(Find(1)->k, 5.50e9) && Find(1)->source == 'B');
  assert(Near(Find(5)->k, 1.10e10) && Find(5)->source == 'C');
  assert(Near(Find(12)->k, 2.20e7) && Find(12)->source == 'D');
  // The two-line rows of the table (R20..R37), checked against the page image.
  assert(Near(Find(20)->k, 3.95e7));
  assert(Near(Find(21)->k, 8.84e9));
  assert(Near(Find(22)->k, 8.00e9) && Find(22)->source == 'B');
  assert(Near(Find(23)->k, 11.0) && Find(23)->unit == Unit::FirstOrder);
  assert(Near(Find(24)->k, 7.52e9));
  assert(Near(Find(29)->k, 1.14e10));
  assert(Near(Find(30)->k, 3.80e10));
  assert(Near(Find(34)->k, 8.40e5));
  assert(Near(Find(36)->k, 0.50));
  assert(Near(Find(37)->k, 0.50));
  assert(Near(Find(41)->k, 0.3));
  assert(Near(Find(53)->k, 2.62e3) && Find(53)->unit == Unit::FirstOrder);
  assert(Near(Find(58)->k, 2.11e-5) && Find(58)->unit == Unit::ZeroOrder);
  assert(Near(Find(59)->k, 1.18e11) && Find(59)->unit == Unit::SecondOrder);
  assert(Near(Find(66)->k, 19.0) && Find(66)->source == 'D');
  assert(Near(Find(73)->k, 1.27e6) && Find(73)->unit == Unit::FirstOrder);
}

static void TestPinnedSpecies()
{
  const Reaction& r7 = *Find(7);
  assert(Is(r7.reactants, {{"e-aq"}, {"O2"}}));
  assert(Is(r7.products, {{"O2-"}}));

  const Reaction& r1 = *Find(1);
  assert(Is(r1.reactants, {{"e-aq", 2}, {"H2O", 2}}));
  assert(Is(r1.products, {{"H2"}, {"OH-", 2}}));

  assert(Is(Find(19)->reactants, {{"OH.", 2}}));
  assert(Is(Find(19)->products, {{"H2O2"}}));
  assert(Is(Find(34)->reactants, {{"HO2.", 2}}));

  assert(Is(Find(16)->products, {{"O2-", 2}, {"H3O+"}}));
  assert(Is(Find(31)->products, {{"HO2."}}));
  assert(Is(Find(43)->reactants, {{"O2-"}, {"HO2-"}}));
  assert(Is(Find(43)->products, {{"O-"}, {"O2"}, {"OH-"}}));
  assert(Is(Find(58)->reactants, {{"H2O", 2}}));
  assert(Is(Find(58)->products, {{"H3O+"}, {"OH-"}}));
  assert(Is(Find(73)->reactants, {{"O-"}, {"H2O"}}));
  assert(Is(Find(73)->products, {{"OH."}, {"OH-"}}));
}

static void TestBulkMarking()
{
  std::set<int> expected = {1, 23};
  for (int i = 58; i <= 73; ++i) {
    expected.insert(i);
  }
  std::set<int> marked;
  for (const auto& r : Reactions()) {
    if (r.bulk) {
      marked.insert(r.id);
    }
  }
  assert(marked == expected);
}

static void TestUnitLabels()
{
  assert(std::string(UnitLabel(Unit::SecondOrder)) == "M^-1 s^-1");
  assert(std::string(UnitLabel(Unit::FirstOrder)) == "s^-1");
  assert(std::string(UnitLabel(Unit::ZeroOrder)) == "M s^-1");
}

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
  TestCountAndIds();
  TestEveryRowIsWellFormed();
  TestPinnedRates();
  TestPinnedSpecies();
  TestBulkMarking();
  TestUnitLabels();
  std::cout << "Tonneau2025TableTest: all tests passed\n";
  return 0;
}
