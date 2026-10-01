/// \file EncounterProbability.hh
/// \brief Exact 3D encounter probability of two diffusing particles with an
///        absorbing sphere of radius R (portable, standard library only).
///
/// Formula (plain doubles, any consistent units; X = D*dt):
///
///   P = [exp(-(r1-R)(r2-R)/X) - exp(-r1*r2/X)] / [1 - exp(-r1*r2/X)]
///
/// Derivation: the separation vector performs 3D Brownian motion with
/// diffusion sum D. With u = r*p the radial process becomes 1D, and an
/// absorbing sphere at R is a Dirichlet condition at u = R. The method of
/// images gives the hitting probability of the bridge from r1 to r2 in time
/// dt. P -> 0 as R -> 0, and P -> P_G4 = exp(-(r1-R)(r2-R)/X) when
/// R*(r1+r2) >> X.
///
/// Geant4's G4DNASmoluchowskiReactionModel uses only the first exponential
/// (P_G4), the large-separation limit, which ignores the reaction rate at
/// short distance.
///
/// Include prefix is rooted at header/; adjust it on copy.
#ifndef EncounterProbability_hh
#define EncounterProbability_hh 1

/// Returns the probability in [0, 1] that two particles at separations r1
/// (start) and r2 (end of the step) have met within the reaction radius.
/// 1 if r1 <= R or r2 <= R; 0 if R <= 0, deltaTime <= 0 or diffusionSum <= 0.
double EncounterProbability(double r1, double r2, double reactionRadius,
                            double diffusionSum, double deltaTime);

#endif
