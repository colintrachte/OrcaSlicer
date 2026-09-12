# Research brief: local versus upstream Windows build parallelism

**Status:** Independent source-grounded comparison
**Date:** 2026-08-29
**Question:** Is this fork's current bounded/adaptive Windows build strategy better or worse than
current upstream OrcaSlicer for a high-core-count desktop?
**Method:** Verified and fetched upstream `main` at
`db29f570bd2f77742ab04e0bb8f0aa55237bd70a`; inspected upstream Windows entry points, CMake
parallel flags, and CI routing; compared them with the local driver, July benchmark evidence, and
Microsoft's documented MSBuild/MSVC concurrency behavior.

## Executive finding

Upstream is **more aggressive and likely faster when abundant memory makes its unbounded
parallelism succeed**, but it is not adaptive or resource-safe. It enables both unbounded MSBuild
project parallelism and unbounded MSVC `/MP`, with no per-target ceiling, learned history, memory
failure classification, or automatic fallback.

This fork's strategy is **better engineered for repeatable local iteration** because it builds
focused targets, bounds the combined concurrency, records outcomes by build identity, and retries
recognized memory failures. However, before the 2026-08-29 fix it was worse in actual use: one old
p2 memory failure permanently forced the Release application target to p1 despite many later p2
successes. That contradicted the benchmark and wasted the owner's hardware.

After the fix, the local approach is the better foundation, but it is not yet proven faster on this
desktop. The current p4 Release run is the first useful higher-ceiling probe. Keep the controls and
fallback, but tune upward from measured CPU/memory headroom rather than copying upstream's
unbounded `-m` plus `/MP` combination.

## What upstream currently does

1. `build_release_vs.bat` configures the full application with `ORCA_TOOLS=ON`, builds
   `ALL_BUILD`, then runs gettext and install. For the Visual Studio generator it invokes
   `cmake --build ... -- -m`; the `-m` is passed through to MSBuild without a numeric limit.[1]
2. Upstream enables `SLIC3R_MSVC_COMPILE_PARALLEL` by default and adds bare `/MP` to Visual Studio
   builds.[2]
3. Microsoft documents that bare MSBuild `-m` uses up to the machine's processor count, while bare
   `/MP` uses the effective processor count for compiler processes.[3][4]
4. Microsoft also documents the combined maximum as `P x C` when MSBuild runs P projects and each
   compiler invocation permits C processes. It recommends measuring and tuning the two dimensions
   for the project's shape.[4]
5. Current upstream CI calls the same Windows batch entry point; no separate Windows concurrency
   policy is supplied by the workflow.[5]

Observed consequence: on a 32-logical-processor desktop, upstream asks both schedulers to use the
machine freely. Actual concurrency depends on ready projects/source files and OS scheduling, so
the theoretical product is not continuously realized, but upstream provides no explicit guard
against oversubscription or PCH/PDB memory pressure.

## What this fork currently does

1. `scripts/build.ps1` keeps one MSBuild project worker and sets an explicit `CL_MPCount` compiler
   ceiling. This is suited to OrcaSlicer's few very large projects and avoids accidental `P x C`
   multiplication.[6]
2. It supports focused application, GUI, core, and exact CMake targets without forcing gettext,
   install, packaging, or unrelated tools.
3. It stores success/resource-failure evidence for the exact target, toolset, architecture,
   configuration, and generator; recognized memory failures retry at a lower ceiling.
4. The 2026-08-29 fix makes old resource failures recoverable after three later successes, selects
   the highest repeatedly proven ceiling, and sends an aggressive attempt to the highest proven
   lower fallback rather than blindly halving past it.[6]
5. The controlled July Release benchmark found p2 about 49% faster than p1 for a representative
   core-header rebuild. Focused RelWithDebInfo targets have subsequently completed repeatedly at
   p8, showing why histories must remain target-specific.[7]

## Better/worse assessment

| Criterion | Upstream | This fork after fix | Better |
| --- | --- | --- | --- |
| Immediate CPU utilization | Unbounded and aggressive | Bounded; current probe p4 | Upstream |
| Memory/PCH stability | No explicit control or retry | Classified failure and proven fallback | This fork |
| Incremental developer loop | Full-oriented batch path | Focused targets; optional configure/install | This fork |
| Hardware-specific learning | None | Exact build-identity history | This fork |
| Recovery from stale evidence | Not applicable | Now rehabilitates old failures | This fork |
| Proven wall time on this desktop | Not measured apples-to-apples | p2 measured; p4 in progress | Undecided |
| Failure cost | Manual restart | Automatic bounded retry | This fork |

The reviewer-owned conclusion is: **our approach is now better overall, but only if it continues
probing upward.** A controller that stays at p2 forever would be safer than upstream but would fail
the owner's throughput goal. Conversely, copying upstream's unbounded double parallelism would
discard the strongest local evidence: OrcaSlicer's huge PCH and translation units can exhaust
memory, and a late failure is very expensive.

## Suggested decision and experiment

1. Retain one MSBuild project worker for the main monolithic application build and tune `/MP` first.
2. Treat p4 as the current bounded probe because the owner observed only 26% total CPU and 62%
   memory use at p3. Do not infer p8 safety from focused RelWithDebInfo targets.
3. If p4 succeeds and its heavy compilation phase retains comfortable memory headroom, use p6 for
   the next owner-requested Release build with p4/p2 fallback. If p4 fails for memory, retain p2 and
   record where in elapsed time it failed.
4. Add phase telemetry before enabling MSBuild project parallelism above one. If later evidence
   shows dependency/small-project phases are the bottleneck, test a fixed total process budget
   split across MSBuild and `/MP`; never leave both dimensions unbounded.
5. Compare successful runs by wall time, object count, peak committed memory, and failure/retry
   cost. CPU utilization alone is diagnostic evidence, not the optimization objective.

## Contradictions and uncertainty

- Upstream's defaults demonstrate that maximum parallelism works often enough for its CI and
  contributors, but upstream source does not provide an apples-to-apples Windows benchmark or a
  documented memory-failure rate for this specific desktop.
- The local July benchmark tested p1/p2 broadly, not p4/p6. The current p4 result is therefore
  essential evidence, not confirmation of a conclusion already reached.
- The owner's 26% CPU/62% memory observation was a point-in-time reading during p3. Resource use
  varies substantially between normal compilation, PCH generation, and LTCG linking.
- No second independent pass is needed yet; the unresolved question is empirical and is best
  answered by the bounded p4 run rather than another source review.

## Sources

1. Upstream Windows build entry point at verified commit, lines 147-177:
   https://github.com/OrcaSlicer/OrcaSlicer/blob/db29f570bd2f77742ab04e0bb8f0aa55237bd70a/build_release_vs.bat#L147-L177
2. Upstream parallel option and `/MP`, lines 131-139 and 351-368:
   https://github.com/OrcaSlicer/OrcaSlicer/blob/db29f570bd2f77742ab04e0bb8f0aa55237bd70a/CMakeLists.txt#L131-L139
   https://github.com/OrcaSlicer/OrcaSlicer/blob/db29f570bd2f77742ab04e0bb8f0aa55237bd70a/CMakeLists.txt#L351-L368
3. Microsoft, MSBuild `-maxcpucount` behavior:
   https://learn.microsoft.com/en-us/visualstudio/msbuild/building-multiple-projects-in-parallel-with-msbuild?view=vs-2022
4. Microsoft, MSVC `/MP`, resource dependence, and `P x C` guidance:
   https://learn.microsoft.com/en-us/cpp/build/reference/mp-build-with-multiple-processes?view=msvc-170
5. Upstream Windows CI call into `build_release_vs.bat`:
   https://github.com/OrcaSlicer/OrcaSlicer/blob/db29f570bd2f77742ab04e0bb8f0aa55237bd70a/.github/workflows/build_orca.yml#L386-L400
6. Local driver: `scripts/build.ps1`, especially `Get-LearnedParallelCeiling`,
   `Get-ProvenParallelCeilingBelow`, and `Invoke-CMakeBuildWithRetry`.
7. Local controlled evidence: `research/architecture-baselines.md`, Windows Release parallelism
   matrix and Phase 2 gate.
