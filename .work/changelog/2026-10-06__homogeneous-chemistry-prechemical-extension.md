---
bump: minor
floor: minor
---
- New `Tonneau2025` Chemistry (`/chem/select Tonneau2025`): the 73-reaction homogeneous pure-water network of Table 2 of Tonneau et al., Phys. Med. Biol. 70 (2025) 235021, run on tracked molecules from the pre-chemical stage instead of from 100 ns. It includes the acid-base block and the dissolved-O2 reactions.
- A Chemistry can now add molecules of its own; `Tonneau2025` adds HO3, which appears as an extra species column in its species and spatial outputs only.
- New example macro `macro/beam_tonneau2025.in` and a 100 ns G-value comparison script.
