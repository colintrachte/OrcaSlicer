# OrcaSlicer improvement dashboard

**Plan:** `research/orcaslicer-improvement-plan.md`
**Current position:** Phase 3 is complete (rename verified, configuration-schema ADR recorded,
both 2026-08-25); Phase 4 (`slic3r_config` pilot continuation, U1 Plater state-machine
characterization) is unblocked and not yet started
**Evidence scope:** Windows x64; macOS/Linux validation deferred by user
**Source baseline:** `1bf7950d8db39201e6940514e3ab6d138c98d360` with the recorded working patch

This dashboard distinguishes observed local evidence from proposed gates. Compilation and tests do
not establish physical print safety.

## Phase checkpoints

| Integrated phase | Status | Evidence or gate |
| --- | --- | --- |
| Phase 0 — reconcile and instrument | Complete | Fixture candidates, metrics, diagnostics, and blocked inputs are named below |
| Phase 1 — correctness net | Complete | G1, C1, and C2 pass their Windows gates |
| Phase 2 — build containment | Complete | Direct-edge budget; retained `GUI_App.hpp` seam; rejected/reverted GUI PCH candidate |
| Phase 3 — configuration/product foundation | Complete | P1, true in-place preset rename, and the configuration-schema ADR (`docs/architecture/configuration-schema-adr.md`) all pass their Windows gates (2026-08-25) |
| Phase 4 — enforceable boundaries | Unblocked, not started | `slic3r_config` target continuation (`docs/architecture/module-boundaries.md` action items 4-5) and U1 Plater state-machine characterization now have their required behavioral oracles from Phases 1 and 3 |
| Phases 5–6 — bounded extraction/expansion | Gated | Run only for a boundary whose prerequisite corpus passes |

## Fixture inventory and provenance

| Candidate | Repository status | Intended use | Decision |
| --- | --- | --- | --- |
| Generated cubes, bridges, overhangs, and multi-volume meshes | Created in test code from project helpers | Primary deterministic support/invalidation fixtures | Preferred; no external asset provenance needed |
| `tests/data/20mm_cube.obj` | Checked in with the initial BambuStudio source import | Canonical loading/slicing smoke case | Usable as an existing repository fixture |
| `tests/data/overhang.obj`, `bridge.obj`, `V.obj`, `V_standing.obj` | Checked in with the initial BambuStudio source import | Small support/overhang geometry candidates | Usable, but generated equivalents are preferred when practical |
| Existing test 3MF/STL files | Checked into this AGPL repository and already exercised by tests | Format compatibility baselines | Retain; do not infer provenance beyond repository history |
| Vendor profiles in `resources/profiles` | Maintained project content | Whole-corpus profile validation only | Do not use as minimal slicing fixtures; set explicit config keys in tests |
| External issue attachments for #4158/#13983 | Not present in the workspace; license/permission not established | Exact bug reproduction | Do not import. Reproduce with generated geometry or obtain explicit provenance first |

The repository license and history make existing checked-in fixtures practical for continued local
testing, but this inventory is not an independent legal opinion. New externally sourced models
remain blocked until their redistribution terms are known.

## Windows baseline

| Metric | Observed value | Notes |
| --- | ---: | --- |
| Warm no-op app | 3.049 s, 0 objects | Final build-iteration check |
| Canonical default slice | 0.349360 s mean (0.345441–0.354657), 3/3 pass | Fixed Catch2 seed; includes process startup; `architecture_metrics_20260731_151211.json` |
| Full profile validation | 8.859787 s mean (8.673253–9.123222), 3/3 pass | 67 vendors loaded; `architecture_metrics_20260731_151211.json` |
| Launcher | 303,104 bytes | Windows Release |
| `OrcaSlicer.dll` | 128,203,776 bytes | Windows Release |
| `libslic3r.lib` | 1,651,417,890 bytes | Informational static archive; debug/path records affect size |
| `libslic3r_gui.lib` | 2,505,374,446 bytes | Informational static archive; debug/path records affect size |

Use `scripts/architecture_metrics.ps1` to refresh artifact, canonical-slice, and profile-validation
measurements. These are non-gating until repeated checkpoints establish variance.

## Current diagnostic baseline

- `[SupportMaterial]` tests pass: 803 assertions across 5 test cases. The two generated organic-tree
  cases add deterministic layer/island summaries, first-layer build-plate contact, polygon validity,
  emitted-extrusion checks, and base/interface filament assignment.
- The canonical default-slice test passes but emits an existing nozzle-info JSON parse error because
  the test runtime does not provide the application resource. Record this noise until the harness
  supplies or explicitly disables that resource path.
- Existing support tests emit `st=1` console noise from current production/test code.
- The generated cantilever now asserts a layer-component reachability graph from the build plate,
  using branch motion plus configured roof-anchor spacing. It also records and locks the fixture's
  diagnostics: zero pending/recovered roof slices, zero lost/very-lost nodes, and 247 deliberately
  discarded empty branch meshes.
- Named tree-support recovery/fallback sites for Phase 1 include lost catch-up area, potentially
  lost branch, failed faster movement, bypass insertion failure, invalid/uninitialized influence
  areas, and no valid model-contact placement in `TreeSupport3D.cpp`, plus unexpected cache-miss
  calculations in `TreeModelVolumes.cpp`.
- The external-focused GUI PCH candidate is rejected and reverted. Ninja remains on hold because
  the Visual Studio 2026 compiler-identification subprocess deadlocks.

## Phase 1 minimum gates

### G1 tree-support corpus

- [x] At least one deterministic generated geometry for organic/tree support.
- [x] Structural summary covers layer count, polygon validity/area, first-layer build-plate contact,
  emitted support, and support/interface material assignment.
- [x] Fixed configuration and Catch2 seed produce identical summaries on repeated runs.
- [x] Component-level connectivity proves every branch reaches the build plate or permitted model contact.
- [x] Expected recovery diagnostics are explicit; unexpected recovery is a failure.

Windows verification on 2026-07-31: the focused target built successfully; `[TreeSupport]` passed
785 assertions in 2 cases on three consecutive fixed-seed runs; the full `[SupportMaterial]` tag
passed 794 assertions in 5 cases. Existing `st=1`, multi-material SupportSpotsGenerator, and missing
test-runtime nozzle-resource diagnostics remain visible and are not attributed to the new fixtures.

Windows closure verification on 2026-08-08: the focused target built successfully; `[TreeSupport]`
passed 794 assertions in 2 cases on two consecutive fixed-seed runs, and `[SupportMaterial]` passed
803 assertions in 5 cases. `[Invalidation]` remained at 82 assertions in 4 cases and
`[GCodeWriter]` at 38 assertions in 6 cases. Existing `st=1`, multi-material
SupportSpotsGenerator, missing nozzle-resource, and missing optional filament-option diagnostics
remain visible. G1 and Phase 1 are complete for the requested Windows scope.

### C1 incremental invalidation

- [x] A table maps representative config families to earliest expected stages.
- [x] Incremental and fresh `Print` outputs agree after normalization.
- [x] Unknown, legacy, and feature-disabled keys retain conservative behavior unless a fixture proves a
  narrower contract.

Windows verification on 2026-08-08: `[Invalidation]` passed 82 assertions in 4 cases with seed
424242. The matrix covers slicing, perimeter count, prepare-infill, infill, support enablement,
support-disabled settings, export-only acceleration, a registered unclassified option, and an
unknown option. The work found and fixed three correctness defects: a function-static support-speed
lambda retained a destroyed `GCode` instance between exports; acceleration settings were classified
as export-only by `Print` but invalidate-all by `PrintObject`; and `wall_loops` reuse from
`posPerimeters` produced four extra extrusion moves versus a clean slice, so its safe boundary is
now `posSlice`. `[GCodeWriter]` passed 38 assertions in 6 cases, and `[SupportMaterial]` remained at
794 assertions in 5 cases. Existing test-runtime diagnostics remain visible.

### C2 configuration compatibility

- [x] Deterministic active-registry export covers IDs/keys, types, defaults, bounds, enum encoding, CLI,
  and feature gating.
- [x] A system-to-user preset inheritance merge and compatibility-expression diagnostics run
  headlessly and repeatably; explicit compatibility lists are also locked as taking precedence.
- [x] Unknown-key explanation and legacy 3MF read/migrate/save/round-trip cases run headlessly and
  repeatably without modifying source artifacts during reads.

Initial Windows registry checkpoint on 2026-08-08: the read-only schema-v1 JSON view contains 759
sorted active options and has FNV-1a digest `0x4fe1dfc4053e23d5`. `[Registry]` passed 17 assertions in
2 cases twice with seed 424242. Adding the source to the existing monolithic core target triggered a
403.9-second CMake/full-core rebuild; the subsequent focused test-only rebuild took 12.7 seconds.
This is baseline evidence for the later `slic3r_config` target gate, not proof that the target split
will pay off.

Preset checkpoint on 2026-08-08: `[Preset][Compatibility]` passed 31 assertions in 2 cases twice
with seed 424242, `[Preset]` passed 146 assertions in 17 cases, and the registry oracle remained at
17 assertions in 2 cases. Malformed compatibility expressions retain the established fail-open
result but now populate the caller's reason string with the expression and parser diagnostic. The
focused Windows rebuild completed in 41.6 seconds; macOS/Linux remain deferred by user direction.

Legacy-format checkpoint on 2026-08-08: the active Orca/Bambu 3MF path now has a generated legacy
project fixture that proves byte-for-byte source preservation on read, in-memory key migration,
unknown-key reporting, explicit modern save, and clean reload. `[3mf][Compatibility]` passed 50
assertions in 1 case repeatedly; `[Config]` passed 79 assertions in 8 cases, `[Registry]` passed 17
assertions in 2 cases, and `[Preset]` passed 146 assertions in 17 cases. The broader explicit
`[3mf]` selector remains unsuitable as a gate because its pre-existing hidden convex-hull case has
an unrelated expected-vertex-count mismatch and writes a debug artifact to Unix `/tmp` on Windows.
C2 is complete for the requested Windows scope.

### P1 preset provenance and compatibility inspector

- [x] Read-only core inspection reports identity, provenance, parent chain, effective-value source,
  dependents, visibility, exact compatibility diagnostics, unknown keys, rejected-load reasons, and
  orphaned `.info` sidecars.
- [x] The GUI can inspect every loaded preset in the active collection, including presets hidden by
  the normal compatibility-filtered selector.
- [x] Representative hidden, incompatible, malformed, broken-parent, rejected-load, unknown-key,
  and orphan-sidecar fixtures are deterministic and preserve their source files byte-for-byte.
- [x] Manual Windows smoke exposed and fixed a dialog-construction crash, theme mismatch, and
  developer-oriented effective-value dump.
- [x] Keep one global warning/overview icon and add context-focused relationship-browser icons to
  the Printer, Process, and Filament editors.
- [x] Add safe, non-destructive recovery-copy actions for fully loaded personal/imported presets.
- [x] Add explicit choices for ambiguous missing-base recovery using backed-up in-place repair.
- [x] Replace the dropdown/report stack with current-relationship and repair-workspace panes.
- [x] Support drag/drop base selection with a button fallback and confirmation.
- [x] Pass the revised interactive Windows smoke gate.

Windows implementation checkpoint on 2026-08-08: `[Inspector]` passed 28 assertions in 2 cases on
repeated seed-424242 runs; `[Preset]` passed 174 assertions in 19 cases; `[Config]` passed 79
assertions in 8 cases; `[Registry]` passed 17 assertions in 2 cases; and the 67-vendor profile
validator completed successfully. The RelWithDebInfo application target compiled and linked after
the full-collection selector was added. The inspector performs only reads; its fixture proves the
source JSON and sidecar remain unchanged.

The archived-case audit corrected the acceptance scope. Issues #7257, #12193, #12223, and #14420
are on-disk/load/compatibility cases addressable by P1. Issues #13967, #14210, and #14396 concern
lost application state or Orca Cloud comparison/synchronization; a local loaded-preset inspector
cannot truthfully explain them without new persistence or cloud diagnostics. They remain explicit
follow-up gates for the later snapshot/conflict work rather than being falsely counted as P1
passes.

Product validation changed the direction from a read-only inspector to preset recovery. The first
dependency-integrity gate completed on 2026-08-08: the one-click printer, filament, and process
exports now materialize the effective configuration and clear the exported copy's hard `inherits`
link, while leaving the live preset unchanged. `[Preset][Export]` passed 12 assertions in 1 case;
the complete `[Preset]` selector passed 192 assertions in 20 cases. The round-trip fixture removes
the source parent and still reloads the exported process preset with inherited and child values;
filament identity and printer-parent detachment are also covered. The RelWithDebInfo application
target compiled and linked successfully. The next gate is dependency-aware bundle/import preflight,
followed by the global recovery UI and transactional repair actions.

Direct-JSON import preflight completed on 2026-08-08. Before the UI importer writes any selected
JSON preset, a read-only planner now identifies its type/name/parent, resolves an installed or
included parent, deterministically orders parents before children, and blocks missing parents,
duplicate definitions, ambiguous parents, or inheritance cycles with a specific message. The
focused `[Preset][Import][Preflight]` selector passed 18 assertions; the complete `[Preset]`
selector passed 210 assertions in 21 cases; and the RelWithDebInfo application target linked.
Archive and bundle preflight completed on 2026-08-08. ZIP, `.orca_printer`, `.orca_filament`, and
`.orca_bundle` files are read into unique temporary paths and the complete archive must pass the
same dependency planner before OrcaSlicer creates its bundle directory or imports its first preset.
Valid archives import in deterministic parent-first order; duplicate basenames cannot overwrite
one another before validation; archive readers, temporary files, and paused bundle reads are now
released on every exit path. The combined `[Preset][Import][Preflight]` selector passed 27
assertions in 2 cases, the complete `[Preset]` selector passed 219 assertions in 22 cases, and the
Windows RelWithDebInfo application target compiled and linked.

Non-blocking compatibility-reference notices completed on 2026-08-08. Import preflight now treats
`inherits` as a hard dependency while checking `compatible_printers` and `compatible_prints` as
soft references. A genuinely unavailable compatibility target does not block loose JSON or archive
imports; it adds a concise notice to the existing import-result dialog explaining that the preset
may remain hidden. Installed renamed targets and targets included in the same selection resolve
without a warning. Notices are capped at five details with a remainder count to avoid a diagnostic
wall. The focused selector passed 39 assertions in 2 cases, the complete `[Preset]` selector passed
231 assertions in 22 cases, and the Windows RelWithDebInfo application target compiled and linked.

The global recovery surface completed on 2026-08-08. The information icon was removed from every
individual settings tab and moved to the main Windows toolbar with the tooltip "Find missing or
unavailable presets." It scans personal/imported printer, process, and filament presets together,
plus rejected-load records, while excluding intentionally disabled system/vendor presets. The icon
changes to a passive red flag and reports the affected count in its tooltip when attention is
needed. The dialog identifies each result by preset category and follows the active FFF/resin
technology. `[Preset][Inspector]` passed 37 assertions in 2 cases, `[Preset]` passed 234 assertions
in 22 cases, and the Windows RelWithDebInfo application target compiled and linked. The next gate
is actionable recovery: safe one-click repairs first, then explicit choices for ambiguity.

Safe recovery-copy actions completed on 2026-08-08. A loaded hidden, incompatible, or orphaned
personal/imported preset now offers a button that creates a uniquely named, visible standalone
copy. The copy retains the already-resolved effective settings, clears hard inheritance, and can
target the currently selected printer/process when compatibility is the problem; the original
preset and file remain unchanged. Files rejected because their parent is missing do not offer an
unsafe detach action: those files contain only differences from the missing parent, so the dialog
explains that OrcaSlicer would have to guess the absent values. `[Preset][Inspector]` passed 51
assertions in 3 cases, `[Preset]` passed 248 assertions in 23 cases, and the Windows RelWithDebInfo
application target compiled and linked. The next gate is explicit user choices for repairs that
have multiple valid targets or would alter an existing preset.

Explicit missing-base choices completed on 2026-08-08. For a rejected diff-only child,
OrcaSlicer now offers up to 12 relevance-ranked, valid loaded bases and requires the user to choose
one. The chosen base supplies the absent inherited values; OrcaSlicer then applies the child's own
differences and writes a uniquely named standalone recovered copy. The rejected source and selected
base remain unchanged, cancellation performs no writes, and a source changed since diagnosis is
refused. `[Preset][Inspector]` passed 63 assertions in 4 cases, `[Preset]` passed 260 assertions in
24 cases, and the Windows RelWithDebInfo application target compiled and linked. The next gate is
an interactive Windows smoke test of the recovery and base-choice dialogs before the configuration
schema ADR.

The first interactive smoke failed on 2026-08-08. The warning asset was a placeholder green
rectangle, refreshing the warning unnecessarily re-realized the proportional top toolbar and moved
the Windows caption controls, and successfully loaded presets carrying forward-compatible fields
were producing non-actionable warnings. Those defects are corrected: the toolbar uses the existing
dependency-chain and orange warning assets, bitmap refresh no longer re-lays out the toolbar, and
unknown fields remain diagnostic without lighting global recovery by themselves. The dialog is now
a left/right relationship workspace: the left tree shows the current affected parent/child path;
the right pane shows concise findings and likely bases. A rejected child may be dragged onto a base,
or the same choice can be made with an accessible button, followed by an explicit confirmation.
Automated Windows verification passes 64 `[Preset][Inspector]` assertions, 261 `[Preset]`
assertions, and the RelWithDebInfo application link. Interactive validation remains open.

The copy-first relationship repair was superseded on 2026-08-09 after product review identified
that an unresolved source warning could generate `(Recovered) 2`, `(Recovered) 3`, and later
duplicates. Missing-parent repair now changes only the existing JSON preset's `inherits` field,
retains its original name, version, and child settings, preserves a byte-identical
`.recovery-backup`, materializes it against the explicitly selected base, and clears the resolved
load issue. A version difference alone never triggers duplication. The relationship workspace is
also a normal preset-management feature: icon-only entries in Printer, Process, and Filament open
the relevant collection focused on the currently selected preset, while the main-toolbar icon
remains the all-category warning overview. `[Preset][Inspector]` passes 71 assertions in 4 cases,
the full `[Preset]` selector passes 268 assertions in 24 cases, `git diff --check` passes, and the
Windows RelWithDebInfo application links. Interactive Windows validation passed on 2026-08-09:
the type-focused Printer, Process, and Filament entry points, relationship layout, backed-up
in-place repair, original-name preservation, restart persistence, and stable caption controls all
behaved as intended.

True in-place preset rename is implemented and Windows-verified (found already complete in code
on re-audit 2026-08-25; the interactive Windows smoke that exercised it left a stray
`base/Renamed Process.json`/`.info` datadir artifact in the repository, since removed).
`rename_user_preset()` in `PresetInspector.cpp` saves the preset under its new name, rewrites
direct children's `inherits`/`base_id`, rewrites cross-type `compatible_printers`/
`compatible_prints`/`default_print_profile`/`default_filament_profile` references and physical-
printer preset links when a `PresetBundle` is supplied, carries forward `setting_id`/`user_id`/
`sync_info`/`updated_time` and appends to `renamed_from`, deletes the old file, and re-selects the
new name — all wrapped in a snapshot/backup-and-restore path that undoes every change if any step
throws, so a failed rename never leaves a half-renamed collection. `Tab::rename_preset()` wires
this to a "Rename this preset in place" toolbar button gated to writable, non-bundled, non-project
presets, with a confirmation dialog naming the affected child count.

Windows re-verification on 2026-08-25: `libslic3r_tests` builds clean (0 warnings introduced, 0
errors). `[Rename]` passes 73 assertions in 9 cases; `[Preset]` passes 367 assertions in 32 cases;
`[Inspector]` passes 126 assertions in 7 cases; all repeated at seed 424242. The dedicated rename
test ("User preset rename updates direct children without leaving a duplicate",
`[Preset][Inspector][Rename]`) covers a parent with a child, a cross-type filament
`compatible_prints` reference, a cross-type printer `default_print_profile` reference, an active
selection with unsaved edits, and a same-name collision against an existing preset — all pass. The
Windows RelWithDebInfo `OrcaSlicer` application target was rebuilt to confirm the link.

Interactive Windows smoke passed 2026-08-25: the owner ran the rebuilt `orca-slicer.exe` by hand
against a scratch `--datadir` and confirmed rename-in-place behaves as intended. True in-place
preset rename is now verified at both the automated and interactive layers.

The remaining Phase 3 exit gate is the configuration-schema ADR (per the plan's own sequencing:
"Record the configuration-schema ADR only after the active contract is observable" — now true,
since C2's registry oracle and P1/rename's preset-mutation contract are both proven).
`docs/architecture/module-boundaries.md` is a different, already-written ADR (the Phase 4
`slic3r_config` compilation-boundary pilot, not the configuration-schema decision); its header
mislabeled itself "Phase 6" and has been corrected to "Phase 4" to match this plan's numbering.
