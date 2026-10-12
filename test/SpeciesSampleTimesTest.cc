/// \file SpeciesSampleTimesTest.cc
/// \brief Plain-assert unit tests for SpeciesSampleTimes (no test framework,
/// no Geant4 runtime -- pure grid/parse/precision logic, times in ns).

// These tests rely on assert(): keep it active even in a build with NDEBUG.
#undef NDEBUG

#include "scoring/SpeciesSampleTimes.hh"

#include <cassert>
#include <cfloat>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

using namespace SpeciesSampleTimes;

namespace
{
  bool Near(double a, double b, double rel = 1e-9)
  {
    return std::fabs(a - b) <= rel * std::fabs(b);
  }

  bool StrictlyIncreasing(const std::vector<double>& v)
  {
    for (std::size_t i = 1; i < v.size(); ++i)
      if (!(v[i] > v[i - 1])) return false;
    return true;
  }

  // Precision for time t: value of the first entry whose key is >= t.
  double PrecisionAt(const std::map<double, double>& m, double t)
  {
    auto it = m.lower_bound(t);
    assert(it != m.end());
    return it->second;
  }

  void TestFixed()
  {
    Setting s;
    s.mode = Mode::Fixed;
    s.step = 2.;
    const auto grid = BuildGrid(s, 10000.);
    assert(grid.size() == 5000);
    assert(grid.front() == 2.);
    assert(grid.back() == 10000.);
    assert(StrictlyIncreasing(grid));
  }

  void TestFixedStepLargerThanEnd()
  {
    Setting s;
    s.mode = Mode::Fixed;
    s.step = 50.;
    const auto grid = BuildGrid(s, 10.);
    assert(grid.size() == 1 && grid[0] == 10.);
  }

  void TestPerDecade()
  {
    Setting s;
    s.mode = Mode::PerDecade;
    s.perDecade = 20;
    const auto grid = BuildGrid(s, 10000.);
    assert(grid.size() == 141);
    assert(grid.front() == 1e-3);
    assert(grid.back() == 10000.);
    assert(StrictlyIncreasing(grid));
  }

  void TestDefault()
  {
    Setting s;
    auto grid = BuildGrid(s, 1000.);
    const std::vector<double> expected = {1e-3, 1e-2, 0.1, 1., 10., 100., 1000.};
    assert(grid == expected);

    grid = BuildGrid(s, 10000.);
    assert(grid.size() == 8);
    assert(grid[6] == 1000. && grid[7] == 10000.);
  }

  void TestList()
  {
    Setting s;
    s.mode = Mode::List;
    s.list = {1., 10., 20000.};
    std::vector<double> dropped;
    const auto grid = BuildGrid(s, 10000., &dropped);
    assert((grid == std::vector<double>{1., 10.}));
    assert((dropped == std::vector<double>{20000.}));
  }

  void TestParseListAccepts()
  {
    std::vector<double> t;
    std::string err;
    assert(ParseList("5 ns", t, err));
    assert(t.size() == 1 && t[0] == 5.);

    assert(ParseList("20 1 1 us", t, err));
    assert((t == std::vector<double>{1e3, 20e3}));

    assert(ParseList("1 ps", t, err));
    assert(Near(t[0], 1e-3));
    assert(ParseList("2 second", t, err));
    assert(Near(t[0], 2e9));
  }

  void TestParseListRejects()
  {
    std::vector<double> t;
    std::string err;
    for (const char* bad : {"", "ns", "1 2 parsec", "1 x ns", "0 1 ns", "-1 ns"}) {
      err.clear();
      assert(!ParseList(bad, t, err));
      assert(!err.empty());
      assert(t.empty());
    }
  }

  void TestPrecisionMap()
  {
    Setting fixed;
    fixed.mode = Mode::Fixed;
    fixed.step = 2.;
    Setting dec;
    dec.mode = Mode::PerDecade;
    dec.perDecade = 20;
    Setting def;
    Setting list;
    list.mode = Mode::List;
    list.list = {1., 10., 20000.};

    for (const Setting* s : {&fixed, &dec, &def, &list}) {
      const auto m = PrecisionMap(*s);
      assert(!m.empty());
      for (const auto& kv : m) assert(kv.second <= 1e-2 * (1. + 1e-12));
      assert(m.rbegin()->first == DBL_MAX);
    }

    const auto mf = PrecisionMap(fixed);
    assert(mf.size() == 1 && Near(mf.begin()->second, 1e-2));

    fixed.step = 0.01;
    assert(Near(PrecisionMap(fixed).begin()->second, 1e-3));

    const double gap = 1e-3 * (std::pow(10., 1. / 20.) - 1.);
    assert(gap > 1.2e-4 && gap < 1.25e-4);
    assert(PrecisionAt(PrecisionMap(dec), 1e-3) <= gap / 10. * (1. + 1e-9));
    // Later decades are capped at 10 ps.
    assert(Near(PrecisionAt(PrecisionMap(dec), 5e3), 1e-2));
  }

  void TestValidate()
  {
    std::string err;
    Setting s;
    assert(Validate(s, err));

    s.mode = Mode::Fixed;
    s.step = 0.;
    assert(!Validate(s, err) && !err.empty());
    s.step = 1.;
    assert(Validate(s, err));

    s.mode = Mode::PerDecade;
    s.perDecade = 0;
    assert(!Validate(s, err));
    s.perDecade = 10;
    assert(Validate(s, err));

    s.mode = Mode::List;
    assert(!Validate(s, err));
    s.list = {1.};
    assert(Validate(s, err));
  }
} // namespace

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif

  TestFixed();
  TestFixedStepLargerThanEnd();
  TestPerDecade();
  TestDefault();
  TestList();
  TestParseListAccepts();
  TestParseListRejects();
  TestPrecisionMap();
  TestValidate();

  std::cout << "SpeciesSampleTimesTest: all tests passed\n";
  return 0;
}
