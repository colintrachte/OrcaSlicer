# OrcaSlicer integrated improvement plan

**Status:** Active; Phase 3 relationship workspace passed Windows validation; true rename next

**Scope:** Core slicing/configuration, GUI/plater/rendering, support generation, and the build and
test infrastructure required to improve them safely

**Primary outcome:** Reduce the probability and cost of regressions in OrcaSlicer's three highest
risk areas while preserving upstream compatibility, cross-platform behavior, profiles, 3MF files,
defaults, and generated G-code.

This plan consolidates, rather than replaces:

- `research/architecture-maintainability.md`;
- `research/architecture-baselines.md`;
- `research/single-most-impactful-orcaslicer-improvement.md`;
- `ai_inbox/orcaslicer-build-iteration-plan.md`;
- `docs/build-iteration.md`; and
- the Infrastructure / CI and Architecture evolution sections of `docs/roadmap.md`.

It adds two guardrails that the earlier architecture sequence did not make prominent enough:

1. explicit incremental-slicing invalidation contracts; and
2. a deterministic tree-support regression corpus.

## Executive decisions

1. **Preserve behavior before decomposing code.** Add characterization, compatibility, and
   differential tests before moving slicing state, support phases, configuration registration, or
   Plater responsibilities.
2. **Keep the existing build-loop gains.** Phases 0-2 of the build-iteration plan are complete.
   Continue to use focused targets, Visual Studio project parallelism of one, compiler ceiling two,
   and the bounded single-worker retry on resource failure.
3. **Run the approved `GUI_App.hpp` pilot next.** Weighted invalidation already selected it over
   `Model.hpp`, `PrintConfig.hpp`, and `Plater.hpp`. Do not substitute a new candidate without new
   measurements.
4. **Treat core invalidation as a correctness contract.** The hand-maintained mapping from changed
   configuration keys to print stages must be tested against clean slicing, not merely refactored.
5. **Treat tree-support fallbacks as observable outcomes.** A slice that continues after losing a
   branch, failing placement, or producing an invalid influence area must be captured by diagnostics
   and regression fixtures.
6. **Use configuration work to produce a user-visible payoff.** The normalized registry,
   compatibility explanations, and preset fixtures should support the previously recommended
   read-only "Why is my preset missing?" inspector before any broad schema conversion.
7. **Pilot boundaries; do not launch a rewrite.** `slic3r_config`, one Plater service, and one
   tree-support phase boundary are independent experiments with explicit exit ramps.
8. **Keep Ninja on hold.** The Visual Studio 2026 Ninja compiler-identification deadlock is a
   recorded toolchain blocker, not an active optimization task.

## Starting baseline

Observed Windows x64 Release results on the controlled Visual Studio 2026 build:

| Scenario | Baseline |
| --- | ---: |
| Warm no-op application build | 3.620-4.168 s |
| Leaf core source | 7.377-9.021 s, 1 object |
| Leaf GUI source | 29.545-32.250 s, 1 object |
| Link only | 15.364-16.104 s |
| `PrintConfig.hpp` change | 671.519-693.565 s, 103 objects at p2 |
| `GUI_App.hpp` change | 1013.181-1043.146 s, 240 objects at p2 |
| Clean application | 2162.402 s, 665 objects |
| Install after clean | 102.646 s, 12 objects |

The weighted graph contains 670 tracked project headers and 46,040 transitive edges. The three
highest-risk areas account for most first-party source and measured compilation cost:

| Area | Risk index | Measured compile share | Principal evidence |
| --- | ---: | ---: | --- |
| Core slicing/configuration | 94/100 | 20.6% plus shared-header fan-out | Manual stage invalidation, caching, compatibility, background execution |
| GUI/plater/rendering | 92/100 | 52.1% | Large coordinators, high header fan-out, background-process state, no direct tests of key classes |
| Support generation | 88/100 | 2.2% | Numerically fragile tree algorithms, recovery fallbacks, minimal focused regression coverage |

These build figures are local trend baselines, not portable CI thresholds. `/Bt+` compiler CPU
figures are useful for relative attribution but not absolute wall-time prediction.

## Prioritized backlog

Priority uses `(impact + risk) x (6 - effort)`, with each input scored 1-5. The formula ranks work
that is ready to start; prerequisite gates still control schedule order.

| ID | Improvement | Debt | Impact | Risk | Effort | Priority | Estimate |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| G1 | Add a pathological tree-support corpus and structural invariants | Test | 5 | 5 | 2 | **40** | 1-2 engineer-weeks |
| B1 | Complete direct-edge analysis and the `GUI_App.hpp` narrowing pilot | Build/architecture | 5 | 3 | 2 | **32** | 1-2 engineer-weeks |
| C1 | Add incremental-versus-clean slicing invalidation tests | Test/architecture | 5 | 5 | 3 | **30** | 2-4 engineer-weeks |
| U1 | Characterize the Plater background-process state machine | Test/architecture | 5 | 5 | 3 | **30** | 2-3 engineer-weeks |
| C2 | Export the normalized active configuration registry and add compatibility fixtures | Test/code | 5 | 5 | 3 | **30** | 3-5 engineer-weeks |
| P1 | Deliver the read-only preset provenance and compatibility inspector | Product/code | 5 | 4 | 3 | **27** | 2-4 engineer-weeks after C2 |
| U2 | Extract a headless slice-job coordinator from Plater | Architecture | 5 | 5 | 4 | **20** | 4-8 engineer-weeks after U1 |
| G2 | Split one tree-support phase behind the regression corpus | Architecture | 4 | 5 | 4 | **18** | 3-6 engineer-weeks after G1 |
| C3 | Pilot the read-only `slic3r_config` target boundary | Architecture/build | 4 | 4 | 4 | **16** | 3-5 engineer-weeks after C2 |
| C4 | Shadow-generate one low-risk setting family | Code/build | 4 | 4 | 4 | **16** | 3-5 engineer-weeks after C2 and the ADR |

Estimates are implementation ranges for one experienced contributor and exclude unattended clean
build time, cross-platform CI latency, upstream conflict resolution, and manual in-app verification.

## Workstream A: correctness guardrails

### A1. Tree-support regression corpus (G1)

Start with the known risky cases already represented in the roadmap, especially:

- organic tree supports whose first layers are missing or print in air;
- multi-material support-interface assignment regressions;
- branch placement that falls back after no valid model contact is found;
- areas lost after collision, bypass, or merge handling; and
- parent-index and connectivity changes after element deletion.

Each fixture should contain a compact generated mesh or checked-in model, a minimal configuration,
and a normalized structural result. Assertions should cover:

- every emitted support component reaches a valid build-plate or permitted model contact;
- supported-overhang coverage does not fall below the recorded baseline;
- layer polygons are valid and have non-negative area;
- branch parent indices remain valid after merging and deletion;
- output is deterministic at a fixed thread/configuration setup;
- material assignment remains correct at support/interface transitions; and
- recovery diagnostics are absent unless explicitly expected by the fixture.

Do not begin by snapshotting complete raw G-code for every case. Prefer structural summaries and
small targeted G-code assertions so harmless formatting changes do not invalidate the corpus.

**Gate:** At least one fixture reproduces each selected failure before its fix. The suite fails if
a support component floats, connectivity is broken, or an unexpected recovery path is taken.

### A2. Incremental slicing invalidation contracts (C1)

Create a representative table:

```text
configuration key -> earliest affected Print/PrintObject stage -> required downstream stages
```

Cover at minimum geometry/slicing, perimeters, infill, support, speeds/flows, wipe tower/tool
ordering, skirt/brim, and export-only changes. For each family:

1. slice a canonical model;
2. apply one configuration change through the normal API;
3. record stages retained and recomputed;
4. produce incremental output;
5. produce output from a fresh `Print`; and
6. compare normalized results and relevant G-code.

Include unknown/legacy keys and feature-disabled behavior. The current conservative invalidate-all
fallback remains until a test proves a narrower mapping.

**Gate:** Incremental and clean slicing agree for every fixture, expected stage invalidation is
observable, and no new option can enter the registry without declaring or explicitly waiving its
invalidation class.

### A3. Configuration, preset, and format compatibility baseline (C2)

Follow the existing architecture roadmap rather than designing a new schema immediately:

- export a deterministic normalized view of the active registry;
- add representative default, enum, bounds, CLI, and feature-disabled fixtures;
- add a system-to-user preset inheritance chain;
- explain unknown keys and failed compatibility expressions;
- add legacy 3MF load, in-memory migration, explicit-save, and round-trip fixtures; and
- keep old files unmodified during reads.

This work creates the oracle used by schema, target, preset-workspace, and upstream-reconciliation
pilots.

**Gate:** The normalized registry and representative compatibility fixtures are deterministic,
headless, and stable across repeated runs.

## Workstream B: build and dependency containment

### B1. Finish the `GUI_App.hpp` pilot

Complete the currently open weighted-invalidation roadmap item before changing PCH or targets:

1. collect direct lexical include/use evidence for `GUI_App.hpp`;
2. classify consumers by required service or global accessor;
3. replace unnecessary includes with forward declarations or narrow interfaces;
4. move non-template implementation out of public headers where practical;
5. repeat no-op, leaf GUI, `GUI_App.hpp`, link-only, and clean scenarios; and
6. record object count, wall time, compiler ceiling/retry, PCH status, and binary size.

Initial pilot review budgets:

- at least 20% fewer transitive consumers or at least 20% lower representative header rebuild
  time;
- no more than 5% clean-build regression;
- no material leaf-source regression;
- no new core-to-GUI dependency;
- no material binary-size increase; and
- no behavior change.

The percentage gates are pilot review thresholds, not permanent universal limits. If repeated
variance makes them misleading, record the variance and use consumer count plus compiler CPU as
the primary decision evidence.

### B2. Developer configurations and PCH

After B1, measure `DevFast`, `DevOptimized`, and `Release` rather than changing global `/Zi`, `/FS`,
or `/LTCG` on intuition. Then run the external-focused GUI PCH experiment described in the existing
build plan.

Do not combine the header pilot, PCH experiment, target split, and compiler-flag changes in one
patch. Each needs a separate before/after attribution.

### B3. Read-only `slic3r_config` target pilot (C3)

Proceed only after C2 provides fixtures. The target must:

- expose a smaller public interface than the current core target;
- keep registration implementation details private;
- allow focused configuration tests to link without wxWidgets;
- preserve initialization, defaults, enum encoding, serialization, and disabled features;
- reduce forbidden dependency edges; and
- improve a representative focused build without unacceptable clean-build or binary-size cost.

Stop at the read-only boundary if moving mutation, migration, or presets would enlarge the public
surface without a demonstrated test or build benefit.

## Workstream C: GUI state and service boundaries

### C1. Characterize Plater background slicing (U1)

Treat `Plater::priv::update_background_process()` and `restart_background_process()` as an implicit
state machine. Document and test event sequences involving:

- model or configuration change;
- plate switch;
- background worker busy/idle transition;
- validation failure and warnings;
- preview or scene invalidation;
- forced slice and export;
- cancellation and restart; and
- FFF versus SLA behavior.

Define outputs as commands rather than widget effects: apply configuration, invalidate result,
reload scene, restart slice, schedule export, and publish diagnostics. Record thread affinity for
every participating state object.

**Gate:** A headless characterization harness can reproduce command ordering and final state for
the selected scenarios without constructing the full application UI.

### C2. Extract one slice-job coordinator (U2)

After U1, move only the characterized responsibility behind a wx-free interface where practical.
Keep Plater as the UI coordinator and preserve the existing worker/event queue and undo transaction
boundary. Do not redesign undo, adopt actors, or move rendering state in the same pilot.

**Gate:** Headless tests pass; manual FFF/SLA slice, cancel, reslice, preview, and export checks pass;
`Plater.hpp` fan-out does not increase; and the extraction creates a useful focused test/build seam.

### C3. Continue GUI decomposition only from evidence

Candidates after the first service pilot are project session, plate repository, export controller,
selection model, printer assignment, and arrangement service. Select the next candidate from
observed churn, fan-out, and defect history. File length alone is not a selection rule.

## Workstream D: support implementation boundaries

After G1 passes, document the data contracts between these phases:

1. overhang/contact discovery;
2. collision and avoidance volume calculation;
3. influence-area propagation;
4. branch merge and placement;
5. layer materialization; and
6. support/interface toolpath generation.

The first extraction should be a pure or near-pure transformation with explicit inputs, outputs,
and diagnostics. It must not change heuristics in the same patch.

**Gate:** The complete support corpus is unchanged, cancellation remains responsive, performance is
within the recorded variance budget, and no fallback diagnostic becomes less visible.

## Workstream E: configuration and preset product payoff

The prior product research selected a transactional Preset Workspace as the single most impactful
user-facing investment. This architecture plan should advance that outcome without coupling it to
a full preset-format or schema rewrite.

### First vertical slice: "Why is my preset missing?" (P1)

Use C2's normalized registry and compatibility diagnostics to add a read-only inspector showing:

- stable identity or current best available identity;
- system/user/vendor provenance;
- parent chain and effective values;
- dependents or affected child presets;
- visibility state and the exact compatibility reason; and
- broken parents, unknown keys, or orphaned sidecars.

Validate against the archived issue cases named in
`research/single-most-impactful-orcaslicer-improvement.md`. Keep the current on-disk formats
readable and unchanged.

**Gate:** Every supplied missing/hidden-preset case can be explained without editing JSON, and the
inspector performs no writes.

The supplied-case gate is scoped to cases observable from loaded presets or preset files. Startup
state loss and cloud-only conflicts/synchronization require evidence not present in the local
collection and remain gates for the later snapshot/conflict work; P1 must not guess about them.

Snapshots, atomic writes, three-way conflict resolution, and cloud/import recovery follow only
after the read-only mental model is proven.

## Delivery sequence

### Execution checkpoint — 2026-07-31

- Phase 0 is complete for the current Windows-only execution. Fixture candidates, provenance
  limits, artifact sizes, canonical slice/profile-validation timings, and existing support
  diagnostics are recorded in `docs/architecture/improvement-dashboard.md`.
- Phase 2 build containment completed ahead of the integrated sequence: the `GUI_App.hpp` seam is
  retained, direct dependency budgets are enforced, and the slower external-focused GUI PCH
  candidate was reverted.
- The first Phase 4 configuration dependency seam (`PresetJsonKeys.hpp`) is retained, but the
  compiled `slic3r_config` target remains held until Phase 1 C2 provides the required registry and
  compatibility oracle.
- Ninja remains on hold. macOS/Linux validation is explicitly deferred by user direction rather
  than represented as passing.
- Phase 1 G1 now has a generated organic-tree cantilever fixture with deterministic structural
  summaries and a two-filament base/interface assignment case. Both pass repeatedly on Windows.
  G1 remains open for component-level connectivity and an explicit recovery-event oracle; C1 may
  proceed independently under the sequencing rule below.

### Execution checkpoint — 2026-08-08

- Phase 1 C1 is complete on Windows. The fixed-seed invalidation suite passes 82 assertions across
  four cases and compares normalized incremental output against a fresh `Print`.
- The representative table covers slice, perimeter, prepare-infill, infill, support, disabled-feature,
  export-only, registered-unclassified, and unknown-key behavior.
- The support-export crash was traced to a function-static lambda capturing a per-export `GCode`
  instance. Removing the static lifetime fixed the dangling access; indexed writer state and caller
  fallbacks retain defensive coverage.
- C1 also aligned acceleration invalidation between `Print` and `PrintObject` and widened
  `wall_loops` to `posSlice` after `posPerimeters` reuse failed clean-output equivalence.
- The active integrated step is C2. G1 remains independently open for component connectivity and an
  explicit recovery-event oracle. macOS/Linux remain deferred by user direction.
- C2 is complete for the requested Windows scope with a deterministic, nonlocalized, read-only registry export. Its first Windows
  baseline contains 759 options with digest `0x4fe1dfc4053e23d5`. A real system-to-user disk-load
  merge and compatibility-expression/list-precedence fixtures pass repeatedly; malformed
  expressions preserve fail-open compatibility while returning a diagnostic. The active
  Orca/Bambu 3MF path also proves read-only legacy loading, in-memory migration, unknown-key
  reporting, explicit modern save, and clean round-trip. The next integrated step is G1 closure.
- G1 is complete for the requested Windows scope. The organic cantilever corpus now proves
  build-plate component reachability with the configured branch/roof span, locks zero unexpected
  roof-recovery and lost-node events, records the expected 247 empty branch-mesh discards, and
  repeats identical summaries and diagnostics at seed 424242. `[TreeSupport]` passes 794 assertions
  in 2 cases and `[SupportMaterial]` passes 803 assertions in 5 cases. Phase 1 is complete; the next
  integrated step is Phase 3 P1, the read-only preset provenance/compatibility inspector.
- P1 is implemented and passes its automated Windows gates. The inspector exposes stable identity,
  provenance, inheritance/effective-value sources, dependents, visibility and compatibility reasons,
  unknown keys, rejected-load diagnostics, broken parents, and orphaned sidecars without writing
  preset files. Its dialog can select any loaded preset in the collection, including presets hidden
  from the normal filtered menu. `[Inspector]` passes 28 assertions in 2 cases repeatedly;
  `[Preset]`, `[Config]`, `[Registry]`, the 67-vendor validator, and the RelWithDebInfo application
  target also pass.
- Archived-case reconciliation assigns #7257, #12193, #12223, and #14420 to P1's observable
  on-disk/load/compatibility scope. #13967, #14210, and #14396 require startup-state or cloud
  diagnostics and remain later snapshot/conflict gates. Manual Windows validation is the decision
  gate before the configuration-schema ADR.
- Manual product validation rejected the technical inspector as the final UX: it was buried in a
  preset tab, initially crashed during dialog construction, exposed an exhaustive effective-value
  dump, and offered no recovery actions. The approved redesign treats export/import integrity and
  global preset recovery as one workflow. Its first gate is complete: standalone printer,
  filament, and process exports are now fully materialized and detached from hard parent
  inheritance. `[Preset][Export]` passes 12 assertions and `[Preset]` passes 192 assertions; the
  Windows RelWithDebInfo application target links. Next: dependency-aware bundle/import preflight,
  then global recovery with safe automatic fixes and interruption levels based on ambiguity and
  reversibility.
- The first import-preflight slice is complete for direct JSON selections. It is read-only until
  the complete selection passes, resolves installed and included parents, orders parent before
  child, and reports missing, duplicate, ambiguous, and cyclic inheritance without importing a
  partial selection. `[Preset][Import][Preflight]` passes 18 assertions and `[Preset]` passes 210
  assertions in 21 cases; the Windows application links.
- Archive preflight now applies the same all-or-nothing dependency planner to ZIP,
  `.orca_printer`, `.orca_filament`, and `.orca_bundle` contents before creating the destination
  bundle directory or importing a preset. Valid contents are imported parent first and duplicate
  archive basenames remain distinct through validation. The combined focused selector passes 27
  assertions in 2 cases, the complete `[Preset]` selector passes 219 assertions in 22 cases, and
  the Windows RelWithDebInfo application target links.
- Missing `compatible_printers` and `compatible_prints` targets are now soft import notices rather
  than blockers. Included and installed/renamed targets resolve silently; genuinely unavailable
  targets are summarized in the existing import-result dialog, capped at five details, and explain
  that the imported preset may remain hidden. `[Preset][Import][Preflight]` passes 39 assertions in
  2 cases, `[Preset]` passes 231 assertions in 22 cases, and the Windows RelWithDebInfo application
  target links.
- Preset recovery is now application-wide instead of embedded in each settings tab. An icon-only
  main-toolbar entry scans personal/imported printer, process, and filament presets plus rejected
  loads, switches to a passive red flag with an issue-count tooltip, and opens category-labelled
  results across the active printer technology. Intentionally disabled system/vendor presets do
  not light the marker. `[Preset][Inspector]` passes 37 assertions in 2 cases, `[Preset]` passes
  234 assertions in 22 cases, and the Windows RelWithDebInfo application links.
- Safe recovery buttons now create a visible standalone copy for fully loaded hidden,
  incompatible, or orphaned personal/imported presets. The copy uses resolved effective settings,
  receives a unique recovered name, and may be scoped to the active printer/process; the original
  remains unchanged. Rejected diff-only children are deliberately not detached because their
  missing parent values cannot be reconstructed without guessing. `[Preset][Inspector]` passes 51
  assertions in 3 cases, `[Preset]` passes 248 assertions in 23 cases, and the Windows
  RelWithDebInfo application links. Next: explicit choices for ambiguous or destructive repairs.
- Ambiguous missing-base recovery now requires an explicit user selection from at most 12 ranked,
  valid loaded candidates. The selected base reconstructs the missing inherited values; the child
  differences are then saved as a uniquely named standalone recovered copy. The rejected source
  file and selected base remain unchanged, and stale source files are refused. `[Preset][Inspector]`
  passes 63 assertions in 4 cases, `[Preset]` passes 260 assertions in 24 cases, and the Windows
  RelWithDebInfo application links. Next: interactive Windows recovery-dialog smoke, then the
  configuration-schema ADR gate.
- The first Windows smoke rejected the icon and dropdown layout. The warning asset was a green
  placeholder, modal close re-laid out the custom toolbar and moved the caption controls, and
  unknown-key-only loaded presets generated warnings with no safe action. These are fixed, and the
  recovery dialog is now a two-pane relationship workspace: current affected parent/child paths on
  the left; concise findings and proposed bases on the right. Dragging the rejected child onto a
  candidate base performs the same confirmed, source-preserving recovery as the button fallback.
  `[Preset][Inspector]` passes 64 assertions in 4 cases, `[Preset]` passes 261 assertions in 24
  cases, and the Windows RelWithDebInfo application links. The revised interactive smoke remains
  the next gate. True in-place preset rename is recorded as a separate follow-on slice because it
  must update dependent names, files, selections, sync metadata, and compatibility references.
- Product review then rejected copy-first missing-parent repair because the unresolved source could
  keep producing numbered recovered copies. The repair now updates only the existing child's
  `inherits` field after an explicit base choice, keeps its name/version/settings, writes a
  byte-identical `.recovery-backup`, and clears the resolved load issue; a version difference is
  diagnostic context, not damage. The same icon-only relationship browser is now available from
  Printer, Process, and Filament and opens filtered to the current type and preset; the top-toolbar
  icon remains the cross-category warning overview. `[Preset][Inspector]` passes 71 assertions in
  4 cases, `[Preset]` passes 268 assertions in 24 cases, and the Windows RelWithDebInfo application
  links. Interactive Windows validation passed on 2026-08-09 for all three type-focused entry
  points, relationship navigation, no-duplicate in-place repair, restart persistence, and stable
  Windows caption controls. True in-place preset rename is the next separate slice.

### Phase 0 - Reconcile and instrument (about 1 week)

- Add this plan to the architecture dashboard.
- Confirm canonical models, profiles, and issue reproductions are legally and practically suitable
  as test fixtures.
- Add missing binary-size and canonical slice-time measurements to the baseline.
- Record current warnings and recovery diagnostics for selected support cases.
- Leave Ninja on hold.

**Exit:** Fixture candidates and metrics are named; no structural code has moved.

### Phase 1 - Correctness net (2-4 weeks)

- G1 tree-support corpus.
- C1 incremental invalidation contracts.
- Begin C2 normalized registry and representative compatibility fixtures.

These can land as separate, buildable changes. Support and invalidation test work may proceed in
parallel, but each fixture must remain small enough to review independently.

**Exit:** Refactors in the three risk areas have automated behavioral or structural oracles.

### Phase 2 - Build containment pilot (1-2 weeks)

- Complete direct-edge attribution.
- Run B1 on `GUI_App.hpp`.
- Update the controlled baseline and dependency budget.

**Exit:** The pilot passes its measured gate or is reverted/stopped with the evidence retained.

### Phase 3 - Configuration and product foundation (3-6 weeks)

- Complete C2 registry export, preset fixtures, and legacy-format fixtures.
- Deliver P1, the read-only preset provenance/compatibility inspector.
- Record the configuration-schema ADR only after the active contract is observable.

**Exit:** Configuration behavior is inspectable and the first user-facing preset problem is solved
without changing persistence.

### Phase 4 - First enforceable boundaries (3-6 weeks)

- Pilot C3, the read-only `slic3r_config` target.
- Complete U1 Plater state-machine characterization.
- Measure focused build/test isolation, binary size, and forbidden edges.

**Exit:** At least one boundary creates a useful focused consumer and passes compatibility/build
gates. Stop any boundary that only rearranges files.

### Phase 5 - Bounded extractions (4-8 weeks each, independently optional)

- U2 headless slice-job coordinator.
- G2 one tree-support phase boundary.
- One shadow-generated low-risk configuration family after the ADR and registry oracle.

Each pilot has an independent exit ramp. None is a prerequisite for shipping unrelated features.

### Phase 6 - Expand only proven patterns

- Apply the successful header technique to `Plater.hpp`, then reconsider core headers.
- Expand target boundaries to presets only if `slic3r_config` paid for itself.
- Expand Plater services only if the first extraction reduced synchronized edits and defects.
- Expand support decomposition only if the corpus remains stable and diagnostics improve.
- Continue toward transactional preset snapshots and conflict resolution if the inspector validates
  the product hypothesis.

## Verification matrix

| Change type | Required verification |
| --- | --- |
| Tests/fixtures only | Focused Catch2 target; prove the intended failure where applicable; repeat for determinism |
| Header/build pilot | No-op, leaf, header, link-only, clean, object count, PCH status, binary size |
| Core invalidation | Incremental-versus-clean normalized output and affected canonical G-code |
| Configuration/preset | Registry differential, inheritance, unknown-key, disabled-feature, profile validation |
| 3MF/format | Legacy open, in-memory migration, explicit save, round trip, source remains unchanged on read |
| Plater/service | Headless state tests plus manual slice/cancel/reslice/preview/export on FFF and SLA paths |
| Support | Structural corpus, material assignment, determinism, cancellation, runtime trend, visual/toolpath inspection |
| Target/PCH architecture | Windows full/targeted build plus proportional macOS/Linux configure or CI evidence |

Compilation and tests do not establish physical print safety. Support, G-code, motion, thermal, and
machine-control changes remain human-review boundaries and require representative print inspection
before release.

## Dashboard

Track these values at each phase checkpoint:

### Correctness

- incremental configuration families covered versus total classified families;
- incremental-versus-clean mismatches;
- tree-support fixtures and invariant failures;
- unexpected support recovery diagnostics;
- legacy 3MF/profile fixtures passing; and
- canonical G-code deltas, approved and unexplained.

### Maintainability

- direct/transitive consumers of `GUI_App.hpp`, `Plater.hpp`, `Print.hpp`, and `PrintConfig.hpp`;
- forbidden target dependency edges;
- GUI modules directly owning slicing/configuration semantics;
- settings requiring edits in more than two ownership locations;
- hand-registered versus shadow-generated settings; and
- upstream merges requiring manual reconciliation in touched architecture files.

### Developer performance

- warm no-op, leaf core, leaf GUI, representative header, link-only, and clean build time;
- objects recompiled and PCH regenerated/reused;
- compiler resource failures and bounded retries;
- focused-test link time;
- core/application binary size; and
- canonical slice runtime trend.

### Product outcome

- supplied hidden/missing-preset cases explained by the inspector;
- invalid profiles diagnosed without opening JSON;
- destructive preset operations covered by snapshots once that phase begins; and
- time to recover a simulated preset import/sync conflict.

## Explicit non-goals and stop conditions

Do not bundle any of the following into these pilots:

- a slicing-engine rewrite or language migration;
- replacement of wxWidgets;
- a new undo architecture, actor framework, or event-sourced application model;
- C++20 modules;
- arbitrary native plugins or a stable external C++ ABI;
- conversion of all settings to a schema;
- a preset-format rewrite;
- maximum target proliferation;
- broad PCH removal without measurement; or
- a local Ninja workaround that cannot be reproduced and supported.

Stop or retain only the useful artifact when:

- a header pilot does not reduce measured invalidation;
- a target split does not create focused tests/builds or narrows no interface;
- a generated schema family cannot match the active registry exactly;
- an extraction increases race, undo, or upstream-merge risk without reducing synchronized edits;
- support decomposition changes behavior before the corpus can explain the delta; or
- recurring upstream reconciliation costs more than the local architecture benefit.

The success condition is not smaller files or more libraries. It is earlier detection of slicing
and compatibility regressions, fewer synchronized edits, faster focused iteration, explainable
preset behavior, and bounded architectural changes that remain easy to merge with upstream.
