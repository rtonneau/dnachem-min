---
bump: minor
floor: minor
---
- SBS is selectable again: `/process/chem/TimeStepModel SBS|IRT_syn`, with the new `/chem/meso/enable true|false` for the mesoscopic stage (SBS and IRT_syn-only run particle-based only; an explicit `true` with SBS is an error).
- The chemistry end time defaults to 1 us without the mesoscopic stage and 1 s with it; `Manifest.json` records `timeStepModel` and `mesoEnabled`, and no `SpeciesMeso*` files are written when the mesoscopic stage is off.
- `sim` finds the macro as given, then next to the executable, then in `macro/`; output goes to `<exe dir>/results` by default, whose `Manifest.json` indexes every dump.
- New example macros `example_sbs.in`, `example_irt.in` and `example_meso.in` (O2 scavenger, two sub-runs dumped to `sub_01` and `sub_02`).
- SBS keeps reactions fully diffusion-controlled and reproduces the recorded SBS reference (`docs/sbs-regression.md`); `Species.Txt` no longer stops at 5 ns without the mesoscopic stage.
