# OrcaSlicer architecture and build-iteration baselines

**Status:** Windows build-iteration baseline recorded; first bounded header pilot retained  
**Controlled runs:** 2026-07-28 baseline; 2026-07-30 `GUI_App.hpp` pilot  
**Scope:** Windows x64, Visual Studio 2026 18.8.12009.203, MSVC 19.51.36248, Release

This document separates observed build-driver results from objectives and unmeasured work. It is
not evidence of slicing correctness, print safety, cross-platform behavior, or release readiness.
The source commit was `1bf7950d8db39201e6940514e3ab6d138c98d360`; the working tree contained
only the build-iteration implementation under evaluation.

## Visual Studio controlled matrix

The application target was `OrcaSlicer_app_gui`. MSBuild project parallelism was fixed at one;
`p1` and `p2` below are compiler-process ceilings. Each repeated value is the arithmetic mean of
three successful runs, with the observed range in parentheses.

| Scenario | p1 | p2 | Recompiled objects | Result |
| --- | ---: | ---: | ---: | --- |
| Warm no-op | 3.973 s (3.620–4.168) | 3.620 s (3.535–3.665) | 0 | stable |
| Leaf core `BoundingBox.cpp` | 8.296 s (7.834–9.021) | 7.745 s (7.377–7.944) | 1 | stable |
| Leaf GUI `AboutDialog.cpp` | 31.066 s (30.372–32.250) | 30.276 s (29.545–30.663) | 1 | stable |
| Link-only application | 15.889 s (15.713–16.104) | 15.580 s (15.364–15.948) | 0 | stable |
| Core header `PrintConfig.hpp` | 1329.321 s, one success; a second run stalled | 682.175 s (671.519–693.565) | 103 | p2 preferred |
| GUI header `GUI_App.hpp` | not run after the p1 core stall | 1026.465 s (1013.181–1043.146) | 240 | 3 successes, 1 failure |
| Isolated clean application | not run | 2162.402 s | 665; PCH regenerated | success |
| Isolated install after clean | not run | 102.646 s | 12 | success |

The p2 core-header mean is 48.7% below the successful p1 result. Successful p2 core-header and
GUI-header runs had coefficients of variation of 1.32% and 1.21%, respectively. The p1
core-header repetition that was stopped is not represented as a successful manifest: its active
compiler had stopped accumulating CPU time at 1609.656 seconds. The next p2 run rebuilt all 103
expected objects successfully, providing interrupted-build recovery evidence.

The fourth p2 GUI-header attempt failed after 1820.741 seconds and 197 objects with MSVC D8040
(`error creating or communicating with child process`). At that point the long-lived PDB service
had a live observed working set of approximately 2.91 GiB. After stopping the idle service, a
subsequent GUI-header run completed in 1043.146 seconds and rebuilt all 240 objects. D8040 is now
classified as a compiler resource failure by the driver so a future occurrence can use the single
bounded retry policy. Because one failure occurred, the strict zero-exhaustion objective is not yet
met even though two workers are the best measured ceiling.

The clean application build completed with zero errors and 29 existing warnings. The install run
reconfigured with `ORCA_TOOLS=ON`, compiled translations, and populated the isolated install tree
with zero errors. The build driver now reports sub-hour durations without rounding fractional
hours upward; manifests were already storing the correct seconds.

Principal evidence:

- no-op: `logs/benchmark_20260728_085942.json`, `logs/benchmark_20260728_090000.json`
- leaf core/GUI: `logs/benchmark_20260728_090051.json`, `logs/benchmark_20260728_090410.json`
- core header p2: `logs/benchmark_20260728_102251.json`, `logs/benchmark_20260728_104602.json`
- link only: `logs/benchmark_20260728_104821.json`
- GUI-header successes: `logs/build_x64_Release_20260728_104833.json`,
  `logs/build_x64_Release_20260728_110550.json`, and
  `logs/build_x64_Release_20260728_115442.json`
- GUI-header failure: `logs/build_x64_Release_20260728_112244.json`
- clean: `logs/benchmark_20260728_125703.json`
- install: `logs/benchmark_20260728_125938.json`

The ignored `logs/` artifacts are local evidence and are not release artifacts.

## Ninja experiment

Ninja Multi-Config did not reach configuration, so no performance comparison is claimed. Both
Strawberry Ninja 1.12.0 and Visual Studio's bundled Ninja 1.13.2 reproduced the failure. CMake's
minimal compiler-identification build launched `cl.exe`; the expected object was written and the
compiler exited, but Ninja then waited indefinitely with zero CPU and no compiler child. The same
result occurred in a real PTY, with and without the shared PDB service and with a Release-mode
try-compile that omitted `/Zi`.

The driver now selects Visual Studio's bundled CMake and Ninja explicitly, but Ninja remains an
unsupported experiment on this VS 2026 toolchain until the process/pipe wait is resolved upstream
or a deterministic local workaround is verified. Visual Studio remains the recommended generator.

## Weighted include invalidation

`scripts/analyze-msvc-includes.ps1` now joins MSVC `CL.read.*.tlog` transitive-consumer edges to
per-translation-unit `/Bt+` frontend/backend timings. The controlled timing build used the same
Windows x64 Release tree. Its p2 attempt failed with D8040 after 2844.797 seconds; the driver's
single bounded p1 retry completed in 5801.607 seconds. The analyzer detected both build sections
and selected only the complete successful retry: 1342 stage records covering 671 sources.

The weighted graph contains 28 tracking logs, 670 project headers, and 46,040 tracked edges. The
leading headers have at least 99.6% timing coverage. `measured invalidation` below is serial
compiler time summed over timed consumers; it is a relative invalidation cost, not predicted p2
wall time.

| Rank | Header | Tracked consumers | Measured invalidation |
| ---: | --- | ---: | ---: |
| 1 | `src/libslic3r/Utils.hpp` | 395 | 83.553 min |
| 2 | `src/libslic3r/ProjectTask.hpp` | 357 | 76.999 min |
| 3 | `src/libslic3r/CustomGCode.hpp` | 345 | 76.869 min |
| 4 | `src/libslic3r/Extruder.hpp` | 344 | 76.480 min |
| 5 | `src/libslic3r/Color.hpp` | 373 | 76.321 min |
| 9 | `src/libslic3r/AppConfig.hpp` | 324 | 74.706 min |
| 27 | `src/libslic3r/Model.hpp` | 249 | 66.296 min |
| 95 | `src/slic3r/GUI/GUI_App.hpp` | 240 | 50.304 min |
| 152 | `src/slic3r/GUI/Plater.hpp` | 170 | 41.749 min |
| 154 | `src/libslic3r/PrintConfig.hpp` | 106 | 32.297 min |

The most expensive individual translation units in the successful sample were `MeshBoolean.cpp`
(54.050 seconds), `PlaceholderParser.cpp` (52.778 seconds), `OrcaSlicer.cpp` (38.987 seconds),
`Plater.cpp` (34.490 seconds), and `bbs_3mf.cpp` (32.331 seconds).

### First header pilot

Select `GUI_App.hpp` for the first bounded interface-narrowing pilot. It has 100% timing coverage,
239 GUI consumers plus the application entry point, and approximately 50.3 serial compiler-minutes
of measured invalidation. It is also a known service-locator dependency with 28 direct includes in
the header itself. This makes it material but more contained than the higher-ranked core headers.

Defer `Model.hpp`: its 249 consumers cross 68 core and 179 GUI translation units, so a first-pass
change has a much larger correctness and architecture surface. Defer `PrintConfig.hpp`: it is
compatibility-sensitive and participates in the GUI PCH. `Plater.hpp` is the next GUI-local
candidate after `GUI_App.hpp`, not a parallel first pilot.

Caveat: `.tlog` data represents compiler-tracked transitive dependencies and may group sources;
it does not identify the lexical edge that introduced a header. The pilot must begin with direct
include/use inspection and must prove a before/after reduction using the same scenarios. Raw local
evidence is in `logs/includes_weighted_x64_Release_20260728.json` and
`logs/slicer_x64_Release_20260728_161157.log`.

### `GUI_App.hpp` pilot result

The bounded pilot moved `HttpServer` ownership behind `std::unique_ptr`, moved non-template
`VersionInfo`, configuration, and message-box bodies into `GUI_App.cpp`, and made
`WebUserLoginDialog.cpp` include `HttpServer.hpp` for the localhost constants it uses. A broader
attempt to remove legacy dialog, preset, device, and ImGui transitive includes was rejected after
the build gate exposed widespread implicit consumer dependencies; those removals are not part of
the retained patch.

With the same Visual Studio Release/x64 application target and p2 compiler ceiling, three
post-change `GUI_App.hpp` rebuilds averaged 960.354 seconds (911.445-985.761), rebuilt 240 objects
each, and completed without a memory retry. The baseline was 1026.465 seconds
(1013.181-1043.146), also at 240 objects. This is a 66.111-second, 6.44% mean improvement.
Three post-change leaf-GUI rebuilds averaged 28.756 seconds (28.182-29.090), versus the recorded
30.276-second baseline, a 5.02% improvement with one object rebuilt each time.

The pilot therefore passes the Phase 5 iteration gate and is retained. Evidence:

- header rebuilds: `logs/benchmark_20260730_223459.json` and
  `logs/benchmark_20260730_230843.json`
- leaf GUI: `logs/benchmark_20260730_223654.json`
- completed affected target: `logs/build_x64_Release_20260730_221750.json`
- completed application target after final source layout:
  `logs/build_x64_Release_20260730_231104.json`
- post-change isolated clean application:
  `logs/benchmark_20260730_234415.json`

The post-change isolated clean application completed in 1902.889 seconds with 665 objects, PCH
regeneration, 29 existing warnings, zero errors, and no retry. The pre-change clean baseline was
2162.402 seconds with the same object count and warning count, so the clean regression guard passes
with a 12.00% lower duration. This is a single clean run, not a stable clean-build speed estimate.

An isolated `git archive HEAD` baseline was configured with the same Visual Studio 2026 generator,
MSVC 19.51 compiler, Release/x64 mode, dependency prefix, optimization, runtime library, language
standard, and PCH policy. `orca-slicer.exe` remained exactly 303,104 bytes. `OrcaSlicer.dll`
decreased from 128,225,792 to 128,203,776 bytes (-22,016 bytes, -0.01717%), so the linked-binary
no-growth guard passes. `libslic3r_gui.lib` decreased from 2,553,257,216 to 2,505,349,614 bytes
(-1.87633%), but that archive comparison is informational because the snapshot's longer absolute
source path may change embedded debug records.

The archive build embeds the seven-character commit abbreviation `1bf7950` because it has no
`.git` directory, while the working build embeds `1bf7950d8d`; this difference is recorded with
the source-path caveat in `logs/artifact_size_gui_app_pilot_20260731.json`. Neither caveat can hide
a material linked-binary increase at the measured scale. Runtime GUI behavior and macOS/Linux
compilation remain unverified for the pilot.

### Direct include attribution and first budget

A single-worker `/showIncludes` capture rebuilt all 240 `GUI_App.hpp` consumers with zero errors,
no retry, and no ambiguous source markers. The compiler-emitted include trees contained 10,276
unique direct edges across the 240 captured sources. Joined with the weighted `.tlog` report, they
show that `GUI_App.hpp` has 160 direct source consumers and 175 direct includers in the active
application scope, versus 240 transitive consumers. Thus at least two thirds of its fan-out is
explicit source-level coupling rather than one removable umbrella-header edge.

`scripts/header-dependency-budgets.json` now freezes the first non-regression ceiling at 240
transitive consumers, 175 direct includers, and 160 direct source consumers. The analyzer accepts
`-BudgetPath`, records pass/fail details in its JSON output, requires direct evidence for direct
budgets, and exits with code 2 on a violation. Weighted invalidation remains an observed baseline,
not a hard budget, because compiler timing variance would make that check unstable.

Evidence:

- capture benchmark: `logs/benchmark_20260731_080724.json`
- compiler include log: `logs/slicer_x64_Release_20260731_073731.log`
- combined budgeted report:
  `logs/includes_direct_weighted_budgeted_x64_Release_20260731.json`

Direct counts for other headers in that report are scoped to the 240 captured `GUI_App.hpp`
consumers and must not be presented as repository-wide counts. `GUI_App.hpp` itself is complete
for the active application target because every one of its tracked consumers was captured.

### GUI PCH pilot

The benchmark harness now has a `GuiPch` scenario that timestamps `src/slic3r/pchheader.hpp` and
records PCH regeneration. With the current GUI PCH and the same Visual Studio Release/x64
application target at p2, the controlled rebuild completed in 1064.58 seconds, regenerated the
PCH, and rebuilt 357 objects (`logs/benchmark_20260731_094722.json`). The pre-candidate linked
artifacts remained 128,203,776 bytes for `OrcaSlicer.dll` and 2,505,349,614 bytes for
`libslic3r_gui.lib`.

The candidate retained platform, STL, Boost, Eigen, TBB, and wxWidgets headers, deduplicated three
entries, and removed the OrcaSlicer project headers from the forced PCH. Under the identical p2
scenario it exceeded the 1200-second execution cap without completing or producing a benchmark
manifest. That is already at least 12.7% slower than the completed baseline, so it failed the
first PCH performance gate. The candidate was reverted exactly; its working-tree blob hash again
matches `HEAD` (`46cd9907d09a795158fce7c91aa3b0b90faffc43`). Leaf, common-header, clean, and post-candidate
artifact comparisons were skipped because they cannot qualify a candidate that already regressed
the primary PCH rebuild workload. No PCH composition change is retained.

## Gate decision

- Phase 0 scenario reproduction: **pass** for the Visual Studio scenarios measured above.
- Phase 1 focused loop: **pass**; leaf edits compiled one object and ordinary development did not
  run gettext or install.
- Phase 2 bounded parallelism: **conditional pass at p2 with p1 retry**; it materially improves
  high-fan-out rebuilds, and the timed clean build demonstrated the one-retry recovery path.
- Phase 3 Ninja generator: **hold** because configuration deadlocks on this toolchain.
- Phase 4 weighted candidate selection: **pass**. Compiler-native direct-edge attribution and an
  enforceable first `GUI_App.hpp` dependency budget are now implemented; expand budgets only with
  equivalent complete capture scopes.
- Phase 5 first bounded header pilot: **pass and retain**; the measured Windows p2 GUI-header mean
  improved 6.44% and the leaf-GUI mean improved 5.02% with unchanged rebuild object counts.
- Phase 5 external-focused GUI PCH pilot: **fail and revert**; the candidate exceeded 1200 seconds
  without completing versus the 1064.58-second current-PCH baseline, a regression of at least
  12.7%. The benchmark scenario is retained, but the PCH composition is unchanged.
- Phase 6 configuration boundary map and first seam: **pass and retain**. `PresetJsonKeys.hpp`
  removes `Config.cpp`'s dependency on the full preset header with all 39 definitions unchanged.
  Three settled config-leaf runs averaged 32.111 seconds versus 32.121 seconds before, and 19/19
  focused config/preset tests passed. The final no-op rebuilt zero objects in 3.049 seconds and the
  application DLL remained exactly 128,203,776 bytes.
- Phase 6 compiled `slic3r_config` target: **hold at gate**. The existing `Config.cpp` object still
  depends on print-schema and application utility symbols, so moving it wholesale would create a
  circular or non-focused boundary.
- Phases 5–7 broader header, PCH, target, or translation-unit changes: **continue on Windows** by
  explicit user direction; macOS/Linux evidence is deferred and is not a blocking gate for the
  current execution. The first pilot's clean-build and linked-binary no-growth guards pass.

Still unverified: peak process/commit telemetry in manifests, broader application/core binary-size
trends, focused runtime tests, and package creation. macOS/Linux configuration or CI is explicitly
deferred by user direction.
