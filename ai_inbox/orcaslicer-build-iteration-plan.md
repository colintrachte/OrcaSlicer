# OrcaSlicer Build-Iteration Improvement Plan

**Status:** Proposed  
**Date:** 2026-07-27  
**Scope:** Windows developer builds first, followed by cross-platform architectural improvements  
**Primary goal:** Make ordinary OrcaSlicer edit-build-test loops fast, predictable, resource-safe, and explainable without weakening release verification or print correctness.

## Executive decision

Retain C++ for the application and slicing engine. Improve iteration speed through four coordinated changes:

1. Build only the requested artifact during development.
2. Bound compiler parallelism safely without multiplying MSBuild and compiler workers.
3. Measure and reduce transitive header invalidation.
4. Introduce narrow CMake targets whose public interfaces form real compilation firewalls.

Python is appropriate for deterministic code generation and build analysis. Rewriting established slicing, geometry, G-code, or wxWidgets code in Rust, Go, C, C#, or TypeScript is not justified as a compile-time optimization.

The plan deliberately separates quick build-driver improvements from architectural work. Target splitting must not proceed on faith: a split is successful only when measured representative builds improve without unacceptable clean-build, memory, binary-size, or compatibility regressions.

## Observed evidence

- The 2026-07-27 successful build manifest recorded 2,433.735 seconds total:
  - configure: 4.899 seconds
  - build: 2,339.389 seconds
  - gettext: 1.213 seconds
  - install: 86.687 seconds
  - parallel limit: one worker
- That run built `ALL_BUILD`, then gettext, then `INSTALL`.
- Earlier diagnostic logs show broad recompilation caused by changed compiler command lines and missing or invalid tracking outputs. The 40-minute result must not be treated as a controlled one-`.cpp` benchmark.
- A previous seven-worker, 49-second build phase followed a long failed build and visibly compiled only two `.cpp` files. It is not comparable to the 40-minute build.
- `libslic3r` contains roughly 213 `.cpp` files; the GUI tree contains roughly 359.
- Approximate direct header consumers in the current tree include:
  - `GUI_App.hpp`: 242
  - `libslic3r.h`: 185
  - `Model.hpp`: 116
  - `PrintConfig.hpp`: 112
  - `Plater.hpp`: 109
- The Windows Release GUI PCH artifact is approximately 743 MB.
- The GUI PCH includes mutable OrcaSlicer headers such as `Config.hpp`, `PrintConfig.hpp`, geometry headers, and project utility headers in addition to STL, Boost, Eigen, wxWidgets, TBB, and platform headers.
- The latest observed GUI rebuild retained its existing PCH but recompiled at least 200 of 357 GUI objects while the working change set contained multiple modified GUI headers. This indicates that ordinary transitive header invalidation is a major cost independent of PCH regeneration.
- `GUI/DeviceCore` is not currently GUI-free despite its CMake comment. It directly uses wx types and includes `GUI_App.hpp`, `Plater.hpp`, `DeviceManager.hpp`, and other GUI utilities.
- Root CMake settings currently apply `/Zi`, `/FS`, and `/LTCG` broadly on MSVC. Their cost and benefit are not separated by developer versus release use.

## Correctness principles

1. A large static-library target does not inherently recompile every source after a leaf `.cpp` edit. Recompilation is controlled by dependency tracking, command-line stability, generated inputs, PCH state, and header fan-out.
2. Target splitting enables focused builds and enforces dependency direction, but does not fix expensive public headers by itself.
3. Header/interface narrowing reduces invalidation, but does not replace targeted build commands or appropriate target structure.
4. Clean, no-op, leaf-source, common-header, and interrupted-build recovery are different workloads and must be reported separately.
5. Build-speed changes must preserve Windows, macOS, and Linux support and must not change generated G-code, profiles, 3MF compatibility, defaults, or feature-disabled behavior.

## Initial performance objectives

These are initial engineering budgets, not claims about current performance. Revise them after controlled baselines exist.

| Scenario | Initial objective |
| --- | ---: |
| No-op target build | under 10 seconds |
| Leaf GUI `.cpp` to updated GUI library | under 60 seconds |
| Leaf GUI `.cpp` to runnable application | under 2 minutes |
| Leaf core `.cpp` to focused test | under 2 minutes |
| Gettext/install/package during normal iteration | zero; explicit opt-in only |
| Compiler or linker memory exhaustion | zero |

## Phase 0 — Make rebuilds explainable

Expand the architecture-baseline work into a reproducible benchmark and invalidation harness.

### Required benchmark scenarios

- clean build
- warm no-op build
- one leaf core `.cpp` timestamp change
- one leaf GUI `.cpp` timestamp change
- one measured high-fan-out core header change
- one measured high-fan-out GUI header change
- link-only rebuild
- interrupted-build recovery
- install/package measured separately
- worker ceilings from one through the safe limit
- Visual Studio generator versus Ninja Multi-Config in separate build directories

### Required manifest fields

- source commit and dirty state
- target requested
- generator, compiler, toolset, architecture, and configuration
- relevant compiler and linker flags
- compiler-command fingerprint
- clean, warm, or interrupted build state
- deliberately changed input
- number of objects actually recompiled
- PCH regenerated or reused
- configure, compile, archive, link, gettext, install, and package times
- MSBuild and compiler worker ceilings
- peak compiler-process count
- available telemetry and memory-related diagnostics
- success, failure, retry, or cancellation

### Unexpected-rebuild diagnostics

Provide an opt-in diagnostic mode that captures an MSBuild binary log and summarizes why targets were out of date. Parse existing MSVC `.tlog` dependency data for normal include-graph analysis. Do not enable `/showIncludes` in the normal parallel build because it is incompatible with MSVC `/MP`; if used, isolate it in a serial analysis configuration.

### Gate

Do not make structural compilation claims until the benchmark can reproduce and distinguish the scenarios above.

## Phase 1 — Ship the fast developer loop

Extend `scripts/build.ps1` with a developer-oriented interface.

### Target selection

Support friendly aliases and exact CMake targets, including at least:

```powershell
-Target Core
-Target Gui
-Target App
-Target libslic3r
-Target libslic3r_gui
-Target OrcaSlicer_app_gui
```

### Phase selection

```powershell
-BuildOnly       # no gettext or install
-Install         # explicit install
-Configure       # explicit configure when needed
-NoConfigure     # use a valid existing build tree
-Run             # run from the build tree where supported
```

The normal developer path must not force `ORCA_TOOLS=ON`, gettext, installation, packaging, or unrelated validation executables unless the selected target genuinely requires them.

### Configure behavior

Avoid an unconditional explicit configure in the hot loop. If a compatible build tree exists, rely on the generated build system's dependency checks to request regeneration when CMake inputs change. Preserve an explicit configure/fresh mode for toolchain, generator, architecture, or material option changes.

### Gate

- A leaf `.cpp` build compiles only the affected object plus demonstrated generated prerequisites.
- Normal development does not run gettext or install.
- Full release and packaging workflows remain available and unchanged in meaning.

## Phase 2 — Safe bounded parallelism

### Version 1: ship with the fast loop

Eliminate multiplicative project/compiler parallelism. Microsoft documents a possible `P × C` process count when MSBuild `/m:P` and compiler `/MP:C` are combined.

For OrcaSlicer's few very large targets, begin with:

```text
MSBuild project parallelism = 1
compiler process ceiling    = conservative per-target value
```

Required controls:

```powershell
-Parallel 0       # automatic safe ceiling
-Parallel 4       # explicit hard ceiling
-MaxParallel 6    # automatic, capped
-SafeBuild        # one compiler process
```

Persist successful and unsafe ceilings by target, toolset, architecture, and configuration. On recognized compiler/linker memory failures such as C1060, C1076, C3859, or equivalent exhaustion:

1. mark the ceiling unsafe;
2. retry at most once at half the ceiling;
3. record both attempts;
4. never enter an unbounded retry loop.

### Version 2: telemetry-driven adaptation

After reliable telemetry exists, consider:

- live Windows commit headroom
- reserved interactive-system capacity
- learned peak commitment per target/PCH phase
- recalculation between dependencies, core, GUI, link, and install phases

Do not delay the `P × C` correction or fast build mode for live resource monitoring. Do not attempt to suspend or manipulate already-running compiler processes.

## Phase 3 — Cheap build-system experiments

### Ninja versus Visual Studio generator

Test early because the experiment is inexpensive, but make no prior claim that Ninja will dominate a compiler/PCH-bound workload.

Requirements:

- separate generator-specific build directories
- identical source revision and material build options
- equivalent clean and warm cache states
- at least three repetitions of no-op, leaf `.cpp`, common-header, and clean scenarios
- report configure, scheduling, compilation, archive, and link time separately

### Developer configurations

Create measured development profiles rather than using Release for every iteration:

- `DevFast`: fastest edit-build-run behavior; reduced optimization; no LTCG
- `DevOptimized`: enough optimization for realistic slicing behavior; no LTCG
- `Release`: final verification and packaging

Evaluate:

- `/Od`, `/O1`, and `/O2`
- `/DEBUG:FULL` versus no final PDB when interactive debugging is unnecessary
- whether `/INCREMENTAL` is actually honored
- `/OPT:NOREF` and `/OPT:NOICF` for an incremental-link profile
- removal of globally forced `/LTCG` outside Release

Do not use `/DEBUG:FASTLINK`; it was removed in Visual Studio 2026.

### Encoding checks

Measure the global encoding-check dependency. Consider a changed-file developer check plus a full-tree verification target if the current all-source check adds material hot-loop cost. Preserve authoritative full validation before integration.

### Gate

Adopt only options that improve representative workloads without invalidating debugging, correctness, or release verification.

## Phase 4 — Build a weighted include/invalidation graph

Use actual compiler dependency data to calculate:

- direct and transitive consumers per header
- aggregate historical compile cost invalidated by each header
- target-boundary crossings
- headers entering each PCH
- which changed input caused each measured object rebuild

Raw consumer count is insufficient. Rebuilding ten template-heavy geometry consumers may cost more than rebuilding fifty small widgets.

### Header dependency budgets

- New public headers must not expose GUI dependencies below the GUI layer.
- Private implementation dependencies belong in `.cpp` files where practical.
- Central headers require explicit direct, transitive, and weighted fan-out budgets.
- Material fan-out increases require justification.
- Prefer lightweight IDs, value types, and abstract interfaces at boundaries.

### Gate

The first header-refactoring candidates must be selected from measured weighted invalidation, not file size or intuition alone.

## Phase 5 — Reduce header and PCH invalidation

Apply bounded changes to measured hotspots:

1. Reduce `GUI_App.hpp` as a general service-locator dependency.
2. Use Pimpl where moving private state actually removes heavy includes.
3. Replace unnecessary includes with forward declarations.
4. Move non-template inline implementation out of public headers.
5. Separate lightweight interfaces from `Model`, `Config`, preset, and GUI implementation details.
6. Add compile-time template firewalls around Boost.Geometry, Eigen, cereal, and JSON only where traces demonstrate benefit.

### PCH pilot

Compare the current PCH against a stable external-focused PCH containing appropriate STL, Boost, wxWidgets, Eigen, TBB, and platform headers but excluding frequently modified OrcaSlicer headers. Consider smaller domain PCH files only after target boundaries exist.

Measure:

- PCH creation time and peak commit
- leaf `.cpp` compilation
- representative common-header rebuild
- clean build
- binary and object-size effects

### Gate

- Leaf-source compilation must not regress materially.
- Representative high-fan-out header rebuild cost or peak commit must improve materially.
- Clean builds must remain within the agreed regression budget.
- Do not change PCH composition solely for conceptual cleanliness.

## Phase 6 — Introduce compilation firewalls

### Pilot 1: read-only `slic3r_config`

The configuration subsystem remains the best first target pilot because its core implementation is currently GUI-free.

Success requires more than moving source files:

- expose a lightweight public interface;
- keep registry/generator implementation details private;
- link focused configuration tests without wxWidgets;
- demonstrate that implementation changes do not recompile GUI consumers;
- preserve initialization, serialization, defaults, enums, CLI metadata, and feature-disabled behavior;
- compare clean build, representative incremental build, and binary size against the baseline.

Stop rather than broaden the split if it adds cost without a useful focused-build/test boundary.

### Pilot 2: presets

Only after the configuration pilot passes, create a narrow preset target for mutation, migration, inheritance, and loading paths. Preserve all profile and 3MF compatibility behavior and prohibit GUI dependency edges below the declared boundary.

### Pilot 3: measured GUI/service candidate

Use the dependency graph to select a low-coupling candidate. `GUI/DeviceCore` is a cleanup candidate, not a ready-made target: first characterize and remove its wx, `GUI_App`, `Plater`, and `DeviceManager` coupling, or choose a lower-cost boundary.

### Gate for every target split

- public headers are narrower than before;
- forbidden dependency edges decline;
- focused builds/tests become possible;
- representative incremental performance improves;
- clean build and binary size remain within agreed budgets;
- behavior and compatibility tests pass.

A target that merely duplicates the same public headers and PCH in another library does not pass.

## Phase 7 — Split large translation units selectively

Use compiler timing to identify large translation units that dominate iteration or prevent useful parallelism. Split by cohesive responsibility, never line count alone.

Candidate responsibilities include:

- `Plater`: project session, plate repository, selection, background slicing, printer assignment
- `GUI_App`: composition/startup services versus presentation/event handling
- `Tab`: configuration model versus UI construction
- `PrintConfig`: generated registry partitions
- `bbs_3mf`: parsing, migration, and serialization

### Gate

- Each extraction has a narrow interface and focused test or build consumer.
- No slicing, G-code, profile, or project compatibility behavior changes without explicit authorization and fixtures.
- Measured compile or test isolation improves.

## Phase 8 — Deterministic generation and caching

### Configuration generation

Use Python to generate deterministic, partitioned C++ implementation data and runtime-safe presentation metadata. Do not generate a single template-heavy public header.

The schema/generator must preserve:

- setting identity and type
- defaults and bounds
- enum encoding
- CLI exposure
- serialization and migration behavior
- gettext identity
- explicitly named native validation and algorithm hooks

### Compiler cache

Pilot `sccache` with MSVC using representative PCH and debug configurations. Measure hit rate, correctness, local-disk cost, branch/worktree reuse, and failure behavior before making it a default.

### Prebuilt core artifacts

Consider content-addressed core artifacts for clean directories, branch changes, and multiple worktrees. A prebuilt-core mode adds little to a healthy same-directory incremental build, where existing objects and libraries already provide reuse.

## Deferred options

### Unity builds

Evaluate only as an opt-in clean-build/CI profile. Unity batches can reduce parsing overhead but may worsen leaf-source iteration and expose ODR problems.

### C++20 modules

Revisit after headers and targets are controlled. Do not combine a modules migration with the initial target or schema pilots.

### Separate slice-engine process

A versioned GUI-to-engine protocol could provide strong build, crash, and test isolation, but only after a narrow in-process slicing interface exists. Otherwise the process boundary would reproduce current coupling as a fragile protocol.

### Shared-library proliferation

Use shared libraries only if measured relinking or process isolation justifies ABI, export, packaging, and cross-platform complexity. Static targets are the default boundary mechanism.

### Other implementation languages

- Python: approved for generators, metrics, validation tooling, and migration tooling.
- Rust: consider only for a new, isolated trust boundary with an independent safety justification.
- Go: consider only for a genuinely standalone service where process isolation and networking simplicity justify another runtime artifact.
- C: do not port established slicing or geometry code for compilation speed.
- C#/TypeScript/web UI: treat as a product/UI redesign, not a build optimization.

## Roadmap ordering

Recommended order in `docs/roadmap.md`:

1. New — Explainable fast developer build and selectable targets.
2. New — Safe bounded Windows concurrency v1.
3. Expand — Record architecture performance, build-time, and binary-size baselines.
4. New — Weighted include/invalidation graph and header dependency budgets.
5. New — Developer compile/link configurations and Ninja experiment.
6. New — Header/PCH firewall pilot.
7. Existing — Map module dependencies and set target budgets before splitting.
8. Existing — Pilot a read-only `slic3r_config` CMake boundary.
9. Existing — Move configuration write and preset-loading paths behind proven targets.
10. New — Characterize and select a GUI/service extraction candidate.
11. Existing/later — Generated schema, caching, and coordinator decomposition.

## Verification requirements

Report exact checks, target/configuration, timings, resource ceilings, and unverified scope for every phase.

Structural/build changes require:

- Windows targeted and full-build verification
- macOS/Linux CMake configuration or CI coverage proportional to the change
- focused unit tests for extracted targets
- no new forbidden dependency edges
- stable benchmark scenarios

Configuration, preset, format, slicing, or G-code-adjacent changes additionally require the roadmap's compatibility fixtures and human-review boundaries. Faster compilation is not evidence of print correctness or physical safety.

## References

- `docs/roadmap.md`
- `research/architecture-maintainability.md`
- `CMakeLists.txt`
- `src/libslic3r/CMakeLists.txt`
- `src/slic3r/CMakeLists.txt`
- `scripts/build.ps1`
- `logs/build_x64_Release_20260727_174630.json`
- `logs/msbuild_vs2026_diagnostic.log`
- [Microsoft `/MP` documentation](https://learn.microsoft.com/en-us/cpp/build/reference/mp-build-with-multiple-processes?view=msvc-170)
- [Microsoft `/DEBUG` documentation](https://learn.microsoft.com/en-us/cpp/build/reference/debug-generate-debug-info?view=msvc-170)
- [CMake unity-build documentation](https://cmake.org/cmake/help/latest/prop_tgt/UNITY_BUILD.html)
- [Mozilla sccache](https://github.com/mozilla/sccache)
