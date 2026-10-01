/// \file EncounterProbabilityTest.cc
/// \brief Plain-assert unit tests for EncounterProbability (no Geant4 runtime).
/// Units: nm / ns (D in nm^2/ns).
#include "chemistry/EncounterProbability.hh"

#include <cassert>
#include <cmath>
#include <iostream>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif

  // Degenerate inputs.
  assert(EncounterProbability(2, 2, 0, 7, 1) == 0.);
  assert(EncounterProbability(2, 2, 0.5, 7, 0) == 0.);
  assert(EncounterProbability(2, 2, 0.5, 0, 1) == 0.);
  assert(EncounterProbability(0.4, 2, 0.5, 7, 1) == 1.);
  assert(EncounterProbability(2, 0.4, 0.5, 7, 1) == 1.);

  // Geant4 limit: R*(r1+r2) >> D*dt, result equals P_G4.
  {
    const double expected = std::exp(-(0.5 * 0.5) / 0.01);
    const double value = EncounterProbability(1, 1, 0.5, 0.01, 1);
    assert(std::fabs(value - expected) <= 1e-12 * expected);
  }

  // Extreme underflow of exp(-r1*r2/(D*dt)): finite, equals P_G4.
  {
    const double value = EncounterProbability(10, 10, 0.5, 0.01, 1);
    assert(!std::isnan(value));
    assert(value >= 0. && value <= 1.);
  }

  // Slow reaction: much smaller than the Geant4 estimate.
  {
    const double geant4 = std::exp(-(1.999 * 1.999) / 7.);
    const double value = EncounterProbability(2, 2, 0.001, 7, 1);
    assert(geant4 > 0.5);
    assert(value < 0.01);
  }

  // Monotonic in R, within [0, 1].
  {
    const double radii[] = {0.01, 0.1, 0.3, 0.6, 1.0};
    double previous = -1.;
    for (double radius : radii)
    {
      const double value = EncounterProbability(2, 2, radius, 7, 1);
      assert(value >= 0. && value <= 1.);
      assert(value > previous);
      previous = value;
    }
  }

  std::cout << "EncounterProbabilityTest: all tests passed\n";
  return 0;
}
