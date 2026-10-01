/// \file EncounterProbability.cc
/// \brief See header/chemistry/EncounterProbability.hh for formula and derivation.
#include "chemistry/EncounterProbability.hh"

#include <algorithm>
#include <cmath>

double EncounterProbability(double r1, double r2, double reactionRadius,
                            double diffusionSum, double deltaTime)
{
  if (reactionRadius <= 0. || deltaTime <= 0. || diffusionSum <= 0.) return 0.;
  if (r1 <= reactionRadius || r2 <= reactionRadius) return 1.;

  const double x = diffusionSum * deltaTime;
  const double full = r1 * r2 / x;
  const double reduced = (r1 - reactionRadius) * (r2 - reactionRadius) / x;

  // exp(-full) underflows to 0 for full > ~700: the result then equals P_G4.
  // expm1 keeps the denominator accurate when full << 1.
  const double denominator = -std::expm1(-full);
  if (denominator <= 0.) return 0.;
  const double probability = (std::exp(-reduced) - std::exp(-full)) / denominator;
  if (std::isnan(probability)) return 0.;
  return std::clamp(probability, 0., 1.);
}
