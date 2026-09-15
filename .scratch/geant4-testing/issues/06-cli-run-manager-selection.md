Status: ready-for-agent

# CLI-driven run-manager-type selection in sim.cc, and fix stale CLAUDE.md claim

Not in the original spec — added mid-implementation after discovering `sim.cc`
doesn't actually match `CLAUDE.md`'s documented behavior. Depends on: 05.

## Context

`CLAUDE.md`'s `sim.cc` bullet currently claims: "Runs multithreaded by
default (`G4RunManagerType::MT`, batch default 4 threads; `/run/numberOfThreads N`
overrides before `/run/initialize`; flip to `Serial` in one line for
reproducible runs)." This is **false as of this session**: `sim.cc:35`
hardcodes `G4RunManagerType::Serial` (MT is commented out on line 33), and the
`SetNumberOfThreads(4)` call on line 42 is gated behind
`runManagerType != G4RunManagerType::Serial`, so it never executes. The
run-manager type is fixed at compile time via
`G4RunManagerFactory::CreateRunManager(runManagerType)` — no macro command can
switch a `Serial` run manager into MT after construction, so
`/run/numberOfThreads` in a macro currently has no effect at all.

Ticket 07 (multi-thread regression case) needs a way to actually run in MT
mode without a separate compiled binary. This ticket adds that.

## Task

1. In `sim.cc`, replace the hardcoded `runManagerType` with a runtime choice:
   accept an optional second CLI argument (thread count), e.g.
   `./sim <macro> [threads]`. When absent or `0`, keep today's default
   exactly (`G4RunManagerType::Serial`, no `SetNumberOfThreads` call) — **the
   default must not change**. When present and `> 0`, use
   `G4RunManagerType::MT` and call `runManager->SetNumberOfThreads(threads)`.
2. Update the log line / comment near the current `runManagerType` declaration
   to describe the new CLI-driven behavior instead of the old hardcoded
   comment.
3. Fix `CLAUDE.md`'s `sim.cc` bullet to describe actual behavior: Serial by
   default; pass a thread count as a second CLI argument to opt into MT.
   Remove the now-false "Runs multithreaded by default" claim and the
   `/run/numberOfThreads` macro-override claim (macros can no longer
   meaningfully use that command to switch modes, since the mode is now fixed
   before the macro executes).

## Acceptance check

Default behavior is unchanged:
```
cd build
./sim.exe beam_02.in
```
Expected: identical console startup behavior to before this change (Serial
mode, no worker-thread log markers), and `Species.Txt` byte-identical to a
pre-change run of the same macro (confirms the default path wasn't altered).

MT opt-in actually switches mode:
```
./sim.exe beam_02.in 4
```
Expected: console output shows Geant4's MT initialization (worker thread
pool / thread-count banner that `G4MTRunManager` prints and `G4RunManager`
does not), distinguishing it from the Serial run above. Runs to completion
with no fatal `G4Exception`.
