Status: ready-for-agent

# CTest setup: compile check + smoke test

Spec: `.scratch/geant4-testing/spec.md` (Implementation Decisions, "Harness
mechanics"; "Compile check"; User Stories 1, 2, 12). Depends on: 03.

## Task

1. Add `enable_testing()` to `CMakeLists.txt`.
2. Register a `compile` CTest case that re-invokes the build
   (`${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config RelwithDebInfo`)
   and expects exit code 0. This is a standalone, `ctest`-discoverable
   re-verification that the project compiles, separate from whatever build
   step happened before `ctest` was invoked.
3. Register a `smoke` CTest case that runs `sim.exe` against the existing
   `beam_02.in` macro (single `/run/beamOn`, no golden-file comparison needed
   — this ticket doesn't require the dedicated test macro from ticket 05) and
   asserts the process exit code is 0 and no fatal `G4Exception` string
   appears in stdout/stderr. A small wrapper script (Python, matching the
   spec's harness-mechanics decision) can run the executable and grep its
   output for `G4Exception` + `FatalException`/`FatalErrorInArgument`, failing
   if either fatal-severity marker is found; non-fatal `G4Exception` warnings
   (e.g. the `UIMAN0123` warning seen this session from a wrong macro path)
   should not fail the test.
4. Wire both into `CMakeLists.txt` via `add_test(...)`.

## Acceptance check

```
cd build
cmake --build . --config RelwithDebInfo
ctest --test-dir . --output-on-failure
```
Expected: output lists 2 tests (`compile`, `smoke`), both `Passed`, e.g.
`100% tests passed, 0 tests failed out of 2`.

```
ctest --test-dir . -R smoke -V
```
Expected: verbose output shows `sim.exe beam_02.in` was invoked and the test
passed with no fatal exception detected.

Negative check — confirm the smoke test actually catches a real failure:
temporarily rename `beam_02.in` to something the test can't find (or point
the test at a nonexistent macro), rerun `ctest -R smoke`, expect `Failed`,
then restore the macro file and confirm `ctest -R smoke` passes again.
