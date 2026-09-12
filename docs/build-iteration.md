# Fast build iteration

The Windows build driver supports a focused developer path without changing the existing full
build/install workflow. A command with `-Target` builds only that CMake target, skips dependency
rebuilds, and does not run gettext or install unless explicitly requested.

## Long-build operating policy

- Use the smallest target and focused deterministic checks that can reject the current patch.
- Build Release only when the owner needs to perform a human slicing test. Automated verification,
  compilation checks, and agent-only work must use focused development targets instead.
- Run long builds quietly in the background with durable stdout/stderr logs. Check once when the
  process exits or when the build reports a failure; do not continuously poll ordinary progress.
- Do not restart a broad build merely to refresh status. Diagnose and resume only after a failure.
- When the owner needs to slice, only Release is suitable. This repository deliberately compiles
  RelWithDebInfo with optimization disabled (`/Od`, `/Ob0`) and runtime checks enabled (`/RTC1`),
  so it is useful for debugging but not a substitute for the owner's slicing build.

## Common loops

```powershell
# Build the runnable application and its required dependencies.
.\scripts\build.ps1 -Target App -BuildOnly

# Rebuild only the GUI or core static library.
.\scripts\build.ps1 -Target Gui -BuildOnly
.\scripts\build.ps1 -Target Core -BuildOnly

# Exact CMake target names are accepted.
.\scripts\build.ps1 -Target libslic3r_gui -BuildOnly

# Reuse a known-compatible generated tree without an explicit configure.
.\scripts\build.ps1 -Target App -BuildOnly -NoConfigure

# Explicitly reconfigure after changing material CMake/toolchain options.
.\scripts\build.ps1 -Target App -BuildOnly -Configure
.\scripts\build.ps1 -Target App -BuildOnly -Fresh

# Build, compile translations, and install explicitly.
.\scripts\build.ps1 -Install
```

`Core`, `Gui`, and `App` resolve to `libslic3r`, `libslic3r_gui`, and the platform application
target. With no developer phase or target option, the historical full configure, build, gettext,
and install workflow remains available.

## Resource ceilings

`-Parallel 0` chooses a conservative compiler ceiling from logical CPUs and physical memory,
capped by `-MaxParallel` (default 6). Visual Studio builds always use one MSBuild project worker;
`CL_MPCount` is the compiler ceiling. This avoids multiplying MSBuild `/m` workers by compiler
`/MP` workers. `-SafeBuild` forces one compiler process.

Successful and memory-unsafe ceilings are learned in the ignored `logs/build_parallel_history.json`
file, keyed by target, toolset, architecture, configuration, and generator. A recognized memory
failure retries at successively halved ceilings until the build succeeds or reaches one compiler
worker; each retry is classified from only its own newly appended log output.

Automatic mode uses the highest ceiling proven by at least three successful builds for the exact
target/toolset/architecture/configuration/generator identity. A memory failure temporarily lowers
that ceiling, but three later successes at the same ceiling rehabilitate it; one stale failure does
not permanently serialize every future build. Use an explicit `-Parallel` value only for a
controlled probe above the proven ceiling.

Throughput policy minimizes expected completion time, not first-attempt success rate. A controlled
probe may start one step above the proven ceiling when the potential time saving justifies a retry.
If it exhausts compiler memory, the retry goes directly to the highest repeatedly proven lower
ceiling rather than blindly halving past it. Failures that occur only near the end of a build remain
expensive, so large jumps above the proven ceiling require new measurements rather than enthusiasm
about logical-core count alone.

The current Windows x64 Release application baseline selects two compiler processes. This is the
highest repeatedly proven stable ceiling for that memory-intensive target and cut the controlled
core-header rebuild by about 49% versus one process. Focused targets use their own history and may
select higher ceilings (p8 is proven for several current RelWithDebInfo test targets). Use
`-SafeBuild` after an unclassified resource failure and retain the generated manifest when reporting
it. D8040 child-process communication failures are treated as resource failures alongside PCH
virtual-memory errors (C3859/C1076 and Windows error 1455) and linker heap exhaustion (LNK1102).

## Diagnostics and benchmarks

`-DiagnoseRebuild` produces an MSBuild binary log, uses diagnostic verbosity, and writes a compact
text summary of common out-of-date reasons. Normal builds retain minimal verbosity and `/MP`.

Use `scripts/benchmark-build.ps1` to label and repeat clean, no-op, leaf-source, common-header,
link-only, interrupted-recovery, and install scenarios in generator-specific build directories.
It preserves deliberately touched input timestamps and writes a summary referencing every build
manifest. Quiet mode redirects the child console without waiting for MSVC's persistent PDB service.

Use `scripts/analyze-msvc-includes.ps1` to summarize tracked transitive consumers from existing
`CL.read.*.tlog` files. Pass one or more compiler logs captured with `/Bt+` to weight each header by
observed frontend/backend translation-unit time:

```powershell
$env:_CL_ = '/Bt+'
try {
    .\scripts\benchmark-build.ps1 -Scenario Clean -Generator VisualStudio `
        -ParallelValues 2 -Repetitions 1 -QuietBuildOutput
}
finally {
    Remove-Item Env:_CL_ -ErrorAction SilentlyContinue
}

.\scripts\analyze-msvc-includes.ps1 -BuildDirectory build-benchmark-vs-x64 `
    -TimingLog logs\slicer_x64_Release_<stamp>.log -HeaderRoot src
```

`/Bt+` materially slows compilation and is for occasional analysis, not ordinary builds. When a
timing build retries, the analyzer selects the final build section and excludes incomplete prior
attempts. Tracking logs remain transitive evidence and do not identify direct lexical include edges.

For compiler-observed direct edges, capture `/showIncludes` with exactly one compiler worker so
translation-unit output cannot interleave, then pass that log to the analyzer:

```powershell
$env:_CL_ = '/showIncludes'
try {
    .\scripts\benchmark-build.ps1 -Scenario GuiHeader -Generator VisualStudio `
        -ParallelValues 1 -Repetitions 1 -QuietBuildOutput
}
finally {
    Remove-Item Env:_CL_ -ErrorAction SilentlyContinue
}

.\scripts\analyze-msvc-includes.ps1 -BuildDirectory build-benchmark-vs-x64 `
    -ShowIncludesLog logs\slicer_x64_Release_<stamp>.log -HeaderRoot src
```

The analyzer reconstructs header-to-header parents from MSVC indentation and reports direct
includers and direct source consumers. Source basenames that are ambiguous in the tracked build
are excluded and counted in the log details.

Pass `-BudgetPath scripts\header-dependency-budgets.json` to enforce the checked-in hotspot
ceilings against the same capture scope. The analyzer writes the observed values and violations
to its JSON output and exits with code 2 when a budget is exceeded. Direct budgets require
`-ShowIncludesLog`; they are not silently skipped when direct evidence is absent.

Every build manifest records the requested/resolved target, generator/toolset/configuration,
compiler and MSBuild ceilings, retry attempts, command fingerprint, conservative recompiled-source
count, observed PCH regeneration, phase timings, and diagnostic artifact paths. Unavailable peak
process and memory telemetry is recorded as null, not zero.

On the tested Visual Studio 2026 toolchain, Ninja Multi-Config stalls during CMake's compiler
identification after the compiler writes its object and exits. The driver deliberately selects
Visual Studio's bundled Ninja, but Visual Studio remains the supported Windows generator until
that blocker is resolved and the controlled matrix can be repeated. See
`research/architecture-baselines.md` for the exact evidence and gate decision.
