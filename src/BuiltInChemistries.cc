/// \file BuiltInChemistries.cc
#include "BuiltInChemistries.hh"

#include "ChemistryRegistry.hh"
#include "PureWaterReactions.hh"

#include "globals.hh"

#include <string>

void BuiltInChemistries::Register()
{
  static bool done = false;
  if (done) {
    return;
  }
  done = true;

  auto add = [](const ChemistryRegistry::Chemistry& chemistry) {
    std::string err;
    if (!ChemistryRegistry::Register(chemistry, err)) {
      G4Exception("BuiltInChemistries::Register", "InvalidChemistry", FatalException,
                  err.c_str());
    }
  };

  add({ChemistryRegistry::kDefaultName, &PureWaterReactions::BuildPureWaterReactions,
       &PureWaterReactions::BuildPureWaterAcidBase});
}
