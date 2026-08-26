- [x] **Score 5 🥇 · Class 2 — Duplicate-canonical-name preset load issues were silent; no
      resolution and no notification (owner-reported, 2026-08-26)** — owner traced a "renamed
      preset disappeared" report to `PresetCollection::load_presets()` (`Preset.cpp:1742-1748`):
      when a preset directory holds two files that resolve to the same canonical name (e.g. one
      in `base/`, one directly in the type folder — confirmed pre-existing on the owner's machine,
      unrelated to any change this session), the loader keeps whichever it scans first and drops
      the other with only a `BOOST_LOG_TRIVIAL(warning)` line — no user-facing signal, no
      resolution path. Renaming this session's own preset-rename feature was cleared as the
      direct cause (no `.rename-backup` was created for the affected file, and its identity
      fields were blank, both inconsistent with `rename_user_preset()`'s own guarantees) —  the
      collision predated it. Added `resolve_duplicate_preset()`/`is_duplicate_name_load_issue()`
      (`PresetInspector.cpp`) to load the shadowed duplicate as a new, distinctly-named preset
      (nothing lost) and mark the original file resolved (renamed, not deleted) so it stops
      re-triggering; wired into the recovery dialog as a "Load as Separate Preset" action.
      `MainFrame::update_preset_recovery_state()` now also pushes a `NotificationManager` toast
      with a "Resolve" hyperlink the first time new preset-load issues appear, instead of only
      lighting a passive toolbar icon the user has to know to check. Also fixed a test-isolation
      bug found while investigating: the existing preset-rename regression test never initialized
      its `PresetCollection`'s directory path, so a rename's *new* file (computed from that path,
      not from any single preset's own `.file`) leaked to `<cwd>/base/<name>.json` — explaining a
      stray `base/Renamed Process.json` artifact found (and removed) in the repo the night before.
      New regression test reproduces the exact collision and resolution; `[Preset]` passes 383
      assertions in 33 cases (up from 367/32), full RelWithDebInfo application target links.

  **Context:** `src/libslic3r/PresetInspector.cpp` · `src/libslic3r/PresetInspector.hpp` ·
  `src/slic3r/GUI/MainFrame.cpp` · `src/slic3r/GUI/MainFrame.hpp`

- [x] **Score 3 🥈 · Class 3 — Vertical brick / interlock layer printing, "Stagger perimeters"
      (#7282)** — 76 rxn, most-requested pure engine feature. Implemented natively rather than
      cherry-picking unmerged community PR #8181 (self-documented alpha, ignores wall slope) or
      wrapping the external `BrickLayers` G-code post-processor (2,198★, the actual community-used
      solution, but text/regex-based and needs the Classic wall generator to avoid Arachne orphaned
      loops). New option alternates the Z-height of inner-wall rings (`inset_idx > 0`; outer wall
      untouched) by absolute layer parity, reusing the existing ZAA (`z_contoured`) per-path Z
      infrastructure rather than adding a parallel mechanism — with ZAA's own height-ratio flow
      auto-compensation explicitly disabled for staggered paths (would have double-compensated
      against the new `stagger_perimeters_extrusion_multiplier`, default 1.05 matching the mature
      script's empirically-tuned value). Adds a slope/top-surface guard (comparing against
      `upper_slices`, mirroring `detect_steep_overhang`'s existing `lower_slices` comparison) that
      prior attempts lacked — a wall segment is only staggered where fully covered by the layer
      above, fixing the documented "staggers even when visible from above" bug. Arachne-only;
      mutually exclusive with Z contouring, spiral vase, and `InnerOuterInner` wall sequence via
      `ConfigManipulation.cpp` auto-fix dialogs. Not compile-verified (no build environment in this
      checkout) — manual review only; compile-check first if a build environment becomes available.

  **Context:** `src/libslic3r/PerimeterGenerator.cpp` · `src/libslic3r/GCode.cpp` ·
  `src/libslic3r/ExtrusionEntity.hpp`

- [x] **Score 5 🥇 · Class 2 — Printer profiles silently lose custom Start/End G-code and Motion
      Ability settings after update (#13075)** — root cause: `PresetCollection::load_presets()`
      deleted the user's `.json`/`.info` preset files from disk on any parse, I/O, or diff-merge
      error during load, turning a transient/recoverable error into permanent data loss. Removed
      the three `fs::remove` blocks; errors are now logged and the preset is skipped for that run,
      leaving the user's file intact.

  **Context:** `src/libslic3r/Preset.cpp`

- [x] **Score 3 🥈 · Class 2 — Auto-placement of added instances breaks down after the first
      copy (owner pain point, 2026-07-05)** — `Plater::increase_instances()` used a fixed diagonal
      offset per new copy with no collision check, so copies beyond the first landed on top of
      existing plate content. Now routes through `GLCanvas3D::get_nearest_empty_cell()`, the same
      collision-aware placement `load_model_objects()` already uses.

  **Context:** `src/slic3r/GUI/Plater.cpp`

- [x] **Score 3 🥈 · Class 3 — Small perimeters speed for supports (#13459)** — touches support
      material and emitted speed values together (Known Risky Subsystem).

  **Context:** `src/libslic3r/Support/TreeSupport.cpp` · `src/libslic3r/GCode.cpp`

- [x] **Score 3 🥈 · Class 2 — Plate toolbar improvements and fixes (#14292)** — needs review;
      scope and bundle-vs-monolith status unknown until the diff is read.

  **Context:** `src/slic3r/GUI/Plater.cpp`

- [x] **Score 3 🥈 · Class 3 — Refactor skirt/brim + bugfixes (#14333)** — title itself signals
      a likely **bundle** (refactor + bugfixes together per `TRIAGE_POLICY.md`'s bundle
      antipattern) — decompose before taking any part. Touches emitted G-code (skirt/brim
      extrusion), so Class 3.

  **Context:** `src/libslic3r/Print.cpp`

- [x] **Score 1 🥉 · Class 2 — Minimal chamber temperature field (#14340)** — small config field
      addition.

  **Context:** `src/libslic3r/PrintConfig.cpp`

- [x] **Score 5 🥇 · Class 2 — Dangling pointer UB in preset recovery dialog**
      — `MainFrame::show_preset_recovery` captures raw `Preset*`/`PresetCollection*` per dialog
      entry, pointing into `PresetCollection::m_presets` (`std::deque<Preset>`).
      `PresetCollection::clone_presets` does a mid-container `m_presets.insert(it, preset)`,
      which invalidates references to every other deque element. If a later dialog action's
      `save()` throws, the dialog stays open with other entries' captured pointers now dangling
      — selecting one is a use-after-free. Fix: capture stable keys (name/index) instead of raw
      pointers, or re-resolve pointers after every mutation.

  **Implement:** `src/slic3r/GUI/MainFrame.cpp` · `src/libslic3r/Preset.cpp` · `src/slic3r/GUI/PresetInspectorDialog.cpp`
  **Context:** `src/libslic3r/PresetInspector.cpp` · `AGENTS.md`

- [x] **Score 5 🥇 · Class 2 — Preset import rollback misses in-memory state**
      — `PresetBundle::import_json_presets`'s rollback `ScopeGuard` restores the pre-import
      backup file on disk when import fails, but `collection->load_preset()` already mutated the
      in-memory `Preset` object in place before that point; nothing reverts it. A failed
      overwrite-import leaves the running session showing the new config while disk holds the
      old one, until restart — silent memory/disk divergence. Fix: also restore in-memory state
      on failure (e.g. reload the collection, or explicitly reassign the pre-import config).

  **Implement:** `src/libslic3r/PresetBundle.cpp`
  **Context:** `src/libslic3r/Preset.cpp` · `AGENTS.md`

- [x] **Score 5 🥇 · Class 2 — Zip archive preset import is now all-or-nothing (data loss)**
      — Archive import sets `extraction_succeeded = false; break;` on the first bad zip entry,
      discarding every already-extracted valid preset from that archive. Previously each entry
      was extracted-and-imported inline, so one bad entry didn't cost the rest. Fix: restore
      per-entry best-effort import (log and skip the bad entry, still import the good ones).

  **Implement:** `src/libslic3r/PresetBundle.cpp`

- [x] **Score 3 🥈 · Class 2 — Import preflight mislabels blocked dependencies as inheritance cycles**
      — The dependency-preflight topological sort never dequeues a node blocked for an unrelated
      reason (e.g. `MissingParent`), so its children's `parent_count` never reaches zero and they
      get wrongly reported as `PresetImportIssueKind::InheritanceCycle` instead of the real
      cause. Fix: exclude children whose unresolved parent is itself already flagged with a
      different issue kind from the cycle-detection leftover check.

  **Implement:** `src/libslic3r/PresetImportPreflight.cpp`

- [x] **Explainable fast developer build and selectable targets**
      — Add friendly/exact target selection, explicit build/configure/install phases, hot-loop
      configure reuse, opt-in rebuild diagnostics, and scenario-rich manifests without changing
      the historical full release workflow.

  **Implement:** `scripts/build.ps1` · `scripts/benchmark-build.ps1`
  **Document:** `docs/build-iteration.md` · `research/architecture-baselines.md`

- [x] **Safe bounded Windows concurrency v1**
      — Prevent multiplicative MSBuild/compiler workers, provide automatic/capped/safe ceilings,
      learn target-specific memory failures, and retry at most once at half the compiler ceiling.

  **Implement:** `scripts/build.ps1`

- [x] **Build a weighted include/invalidation graph and header dependency budgets**
      — Extend the tracked transitive-consumer graph with direct compiler include evidence,
      per-translation-unit compile cost, target crossings, and PCH membership before selecting
      structural refactors. `/Bt+` weighting, target/domain/PCH annotation, single-worker
      `/showIncludes` direct attribution, and an enforceable `GUI_App.hpp` dependency budget are
      complete. Expand checked budgets only from equivalent complete capture scopes.

  **Implement:** `scripts/analyze-msvc-includes.ps1`
  **Context:** `research/architecture-baselines.md`

- [x] **Run the header/PCH firewall pilot**
      — Compare the existing GUI PCH with a stable external-focused candidate only after weighted
      invalidation data identifies representative workloads and clean-build regression budgets.

  The first bounded header seam is complete: `HttpServer.hpp` was removed from `GUI_App.hpp` via
  private ownership, cutting the controlled 240-object GUI-header rebuild mean by 6.44% and the
  one-object leaf-GUI mean by 5.02%. Its isolated 665-object clean build was 12.00% below baseline
  with the same warning count; the executable was unchanged in size and the application DLL was
  0.017% smaller. A subsequent external-focused GUI PCH candidate exceeded 1200 seconds without
  completing versus the 1064.58-second current-PCH baseline, so it failed the first performance
  gate and was reverted. The reusable `GuiPch` benchmark scenario is retained; the PCH content is
  unchanged.

- [x] **Score 3 🥈 · Class 1 — Map module dependencies and set target budgets before splitting**
      — Document current configuration/preset incoming and outgoing edges, initialization order,
      public headers, PCH assumptions, and forbidden GUI dependencies. Define success budgets for
      focused GUI-free linking, clean/incremental build trends, and binary size; do not change
      target ownership yet.

  **Read:** `src/libslic3r/CMakeLists.txt` · `src/CMakeLists.txt` · `tests/libslic3r/CMakeLists.txt` · `src/libslic3r/PrintConfig.cpp` · `src/libslic3r/Preset.cpp`
  **Write:** `docs/architecture/module-boundaries.md`
  **Route:** claude
  **Effort:** 4834
  **Chars:** ~805,692 total (largest: src/libslic3r/PrintConfig.cpp ~567,848) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [x] **Score 5 🥇 · Class 3 — Ensure spiral lift stays inside print area, avoid nozzle collision (#13634)**
      — Root cause: `eager_lift`/`travel_to_xyz`'s `LiftType::SpiralLift` path always traced a
      full circle (`_spiral_travel_to_z`, radius = `z_hop / (2*PI*tan(travel_slope))`) with two
      `// TODO: check the arc will move within bed area` markers left unaddressed - a shallow
      `travel_slope` can put that radius at tens of mm, large enough to swing outside a small bed
      and strike the frame. Fix: `GCodeWriter` now caches the bed polygon (`m_bed_shape`, from
      `apply_print_config`'s `get_bed_shape`) and `is_spiral_lift_within_bed()` samples 32 points
      around the would-be circle against it; either spiral call site falls back to a straight lift
      (`_travel_to_z`) instead of the arc when any sample would land outside the bed. Build-
      verified (`libslic3r` target, RelWithDebInfo) with new regression coverage in
      `tests/fff_print/test_gcodewriter.cpp` (small bed forces the fallback, large bed still uses
      the spiral); full `fff_print_tests` suite run clean relative to the pre-fix baseline (same
      3 pre-existing `wall_loops` incremental-slicing failures, confirmed unrelated by reproducing
      them on the baseline). Not yet verified end-to-end in the GUI (slice preview / real print).

  **Context:** `src/libslic3r/GCodeWriter.cpp`

- [x] **Score 5 🥇 · Class 3 — Multimaterial (IFS) broken on Flashforge Adventurer 5X — owned hardware (#10783)**
      — DONE verification (2026-08-24, per TRIAGE_POLICY.md's re-entry rule): the referenced
      community PR was found, read, and is **already merged into this fork's HEAD** —
      OrcaSlicer/OrcaSlicer#12991 (marianomd, merged upstream 2026-05-27) landed here as commit
      `f118b6b337` ("Add Flashforge Adventurer 5 series local send workflow with IFS mapping"),
      confirmed an ancestor of current HEAD via `git merge-base --is-ancestor`. It adds local
      AD5X discovery, an IFS slot-selection send dialog
      (`FlashforgeSlotDialog`/`FlashforgeMaterialMapWidget` in `PrintHostDialogs.cpp`), material-
      station slot reporting (`Flashforge::fetch_material_slots`), and switches uploads to 3MF
      packaging matching the AD5X's expected workflow — directly addresses the issue's root cause
      (Orca not telling the printer which IFS slot to use, causing false runout errors). No code
      change needed; closing as already shipped rather than re-implementing.

  **Context:** `src/slic3r/Utils/Flashforge.cpp` · `src/slic3r/Utils/Flashforge.hpp`

- [x] **Score 5 🥇 · Class 3 — Bridges sliced as overhang walls instead of using bridge flow/density (#2231)**
      — Cherry-picked (2026-08-24, per `TRIAGE_POLICY.md`'s Cherry-pick tier) from
      OrcaSlicer/OrcaSlicer#14216 (stefansjs, "Fix bridge perimeter extrusion roles", open,
      mergeable, not a draft). Root cause: bridge perimeters (the wall segments spanning an
      unsupported void) were tagged `erOverhangPerimeter`, the same role as ordinary overhangs,
      so they got overhang speed/flow/fan instead of the user's dedicated bridge settings. Fix
      adds a new `erBridgePerimeter` extrusion role, assigned from the existing overhang-split
      geometry in `PerimeterGenerator` for the first bridge layer, plus a new post-pass
      (`PrintObject::tag_extra_bridge_perimeters` / `split_perimeter_over_void`) that walks down
      to the nearest non-bridge layer to correctly tag perimeters on *stacked* extra bridge
      layers, which `PerimeterGenerator` can't see (they look "supported" by the bridge layer
      directly below). The new role is threaded through flow/speed/fan/PA G-code emission,
      `FanMover`, `PressureEqualizer`, support-material bridging detection, and the G-code
      viewer's role coloring.
      **Excluded from the cherry-pick:** the PR's one-line `MeshBoolean.cpp` hunk (an unrelated
      CGAL `triangulate_hole` API call-signature change with zero relation to bridging — a bundle
      antipattern per `TRIAGE_POLICY.md`) was deliberately left out; that file is untouched.
      **Conflict resolved:** one 3-way merge conflict in `GCode.cpp` (this fork had already
      renamed `get_nozzle_config_index(...)` to `cur_extruder_index()`) — resolved by keeping the
      fork's helper name and adding the PR's new `erBridgePerimeter` check alongside it.
      **Verified:** `fff_print_tests` builds clean; all 24 new test cases from the PR
      (`test_bridge.cpp`, `test_bridge_params.cpp`, `test_bridge_perimeter_fan.cpp`,
      `test_extra_bridge_fan.cpp`, plus `test_bridge_helpers.hpp` and two new bridge test
      meshes) pass (101 assertions); full suite (89 cases) shows the same 3 pre-existing
      `wall_loops` incremental-slicing failures as the pre-fix baseline (unrelated, see #13634's
      shipped entry for how that was confirmed) — zero regressions from this change.
      **Not included:** #12470 ("Improve thick bridges spacing") is a separate, still-open
      sub-issue (a different root cause — the `BRIDGE_EXTRA_SPACING` constant in `Flow.cpp`
      governing thick-bridge line spacing) - not part of this cherry-pick; revisit separately if
      thick-bridge spacing is still a live complaint.

  **Implement:** `src/libslic3r/ExtrusionEntity.cpp` · `src/libslic3r/ExtrusionEntity.hpp` ·
  `src/libslic3r/GCode.cpp` · `src/libslic3r/GCode/FanMover.cpp` ·
  `src/libslic3r/GCode/GCodeProcessor.cpp` · `src/libslic3r/GCode/PressureEqualizer.cpp` ·
  `src/libslic3r/PerimeterGenerator.cpp` · `src/libslic3r/Print.hpp` ·
  `src/libslic3r/PrintObject.cpp` · `src/libslic3r/Support/SupportMaterial.cpp` ·
  `src/libslic3r/Support/SupportSpotsGenerator.cpp` · `src/libslic3r/VariableWidth.cpp` ·
  `src/libvgcode/include/Types.hpp` · `src/libvgcode/src/Settings.hpp` ·
  `src/libvgcode/src/ViewerImpl.cpp` · `src/slic3r/GUI/GCodeViewer.cpp` ·
  `src/slic3r/GUI/LibVGCode/LibVGCodeWrapper.cpp`

- [x] **Score 5 🥇 · Class 3 — Wipe tower extruder override respected in multimaterial (#10971)**
      — Shipped by upstream fix #12108 (`ed3f0e2898`): the configured tower filament is included
      in tool ordering and honored by both wipe-tower implementations. The later upstream tower
      sync was reconciled in this fork, including the classic-tower wall-selection repair in
      `1f03ce36bd`. The full application builds and the focused wipe-tower tests pass.

  **Implement:** `src/libslic3r/Print.cpp` · `src/libslic3r/GCode/ToolOrdering.cpp` ·
  `src/libslic3r/GCode/WipeTower.cpp` · `src/libslic3r/GCode/WipeTower2.cpp`

- [x] **Score 3 🥈 · Class 2 — Stop OrcaSlicer instances from stealing window focus (#9874)**
      — Upstream fix #10374 (`eb92a2ddde`) removed the deactivation-time
      `RequestUserAttention` calls that randomly reactivated dialogs in another instance.

  **Implement:** `src/slic3r/GUI/ReleaseNote.cpp`

- [x] **Score 3 🥈 · Class 2 — Connect to Elegoo Centauri Carbon 2 (#13550)**
      — The roadmap's identified implementation PR #13212 (`427d0f7a9f`) is in the current
      history. It adds CC2 file upload and device-details support; current profiles and the
      Elegoo G-code helper also recognize the Centauri Carbon 2 family.

  **Implement:** `src/slic3r/Utils/ElegooLink.cpp` · `src/slic3r/Utils/ElegooLink.hpp`

- [x] **Score 3 🥈 · Class 3 — Tool changer supports multiple nozzle sizes in one print (#11424)**
      — Shipped through upstream #12824 (`afc3756843`), including a real Snapmaker U1
      0.4+0.6-mm mixed-nozzle profile and toolchanger-aware material/nozzle handling. The
      separate non-BBL nozzle-selector UI request (#14145) remains open.

  **Context:** `src/libslic3r/GCode/ToolOrdering.cpp` · `src/libslic3r/PrintConfig.cpp` ·
  `resources/profiles/Snapmaker/`

- [x] **Score 5 🥇 · Class 2 — Export the normalized active configuration registry**
      — `ConfigRegistry` now emits deterministic, nonlocalized JSON for the active registry,
      including keys, types, defaults, bounds, enum encodings, CLI exposure, aliases, modes,
      nullability, serialization ordinals, ratio bases, and printer technology. Golden-style
      tests pin both the option count and content hash; the merged registry currently contains
      827 options and passed the complete core test suite.

  **Implement:** `src/libslic3r/ConfigRegistry.cpp` · `src/libslic3r/ConfigRegistry.hpp`
  **Verify:** `tests/libslic3r/test_config_registry.cpp`

- [x] **Score 5 🥇 · Class 2 — Add focused configuration and preset compatibility fixtures**
      — Added deterministic registry-contract checks plus preset fixtures covering
      system-to-user inheritance, renamed parents and compatibility lists, unknown/dangling
      references, explicit compatibility explanations, SLA compatibility routing, and library
      profile masking. These cases pass in the complete core suite.

  **Verify:** `tests/libslic3r/test_config_registry.cpp` ·
  `tests/libslic3r/test_preset_bundle_loading.cpp`

- [x] **Score 5 🥇 · Class 2 — Add versioned 3MF migration and round-trip fixtures**
      — The compatibility fixture creates a legacy project using the former
      `enable_wipe_tower` key, verifies migration occurs only in memory, proves the source bytes
      remain unchanged, explicitly saves the current representation, and reloads it to verify
      the approved round trip. It passes in the complete core suite.

  **Verify:** `tests/libslic3r/test_3mf.cpp`

- [x] **Score 1 🥉 · Class 3 — Top/bottom fill order control (Outward/Inward) (#14179)**
      — Merged upstream as `c84d5c7943`, adding selectable path order for Concentric,
      Archimedean Chords, and Octagram Spiral top/bottom surfaces.

  **Implement:** `src/libslic3r/Fill/FillBase.cpp` · `src/libslic3r/LayerRegion.cpp`

- [x] **Score 3 🥈 · Class 3 — Retract amount after wipe (#11015)**
      — Merged upstream as `50768e0716`, adding the filament retract-after-wipe configuration
      and threading it through the retraction/G-code writer path.

  **Implement:** `src/libslic3r/Extruder.cpp` · `src/libslic3r/GCodeWriter.cpp`

- [x] **Score 5 🥇 · Class 3 — Ported wipe-tower feature fixes (#12742)**
      — Superseded by separately integrated upstream work and the later full BambuStudio tower
      sync (`7a378d2fc4`). Skip-point gap walls, interface pre-extrusion, tower ironing, and rib
      walls are all present in both the active configuration and tower code. The merged tower
      builds and its focused tests pass; the bundled PR itself was not cherry-picked wholesale.

  **Implement:** `src/libslic3r/GCode/WipeTower.cpp` · `src/libslic3r/GCode/WipeTower2.cpp`

- [x] **Score 5 🥇 · Class 3 — Floating final purge line fix (#14416)**
      — Merged upstream as `b2a7197a07`, keeping the final purge line attached to the intended
      wipe-tower geometry.

  **Implement:** `src/libslic3r/GCode.cpp` · `src/libslic3r/Print.cpp`

- [x] **Score 3 🥈 · Class 2 — Keep OrcaFilamentLibrary printer-specific siblings visible (#13573)**
      — Superseded by upstream #15101 (`74c4a7e450`). Printer-specific library profiles now
      supersede a generic profile sharing their alias without being hidden themselves, with a
      regression test covering both library-owned and vendor-owned variants.

  **Implement:** `src/libslic3r/Preset.cpp`
  **Verify:** `tests/libslic3r/test_preset_bundle_loading.cpp`

- [x] **Score 3 🥈 · Class 3 — Anisotropic surfaces + Separated Infills (#11682)**
      — Merged upstream as `bc6ffcfb31`, followed by the separated-infill enhancements in
      `c4ea86041b`.

  **Implement:** `src/libslic3r/Fill/Fill.cpp` · `src/libslic3r/Fill/FillBase.cpp`

- [x] **Score 3 🥈 · Class 3 — Cyclic ordering toolpath strategy (#13578)**
      — Merged upstream as `ef7bfeda9c`, with subsequent improvements in `33dfb66aa5` and the
      Standard/Cyclic configuration control in `da149ee75a`.

  **Implement:** `src/libslic3r/GCode/OrderingStrategies.cpp` · `src/libslic3r/GCode.cpp`
  **Verify:** `tests/libslic3r/test_ordering_strategies.cpp`

- [x] **Score 3 🥈 · Class 2 — Cache system presets to eliminate startup/wizard load time (#14217)**
      — Merged upstream as `5ed56eb876`. System presets are serialized into validated vendor
      cache files and used by startup/wizard loading, with fallback behavior and regression
      coverage. The merge-specific loader repairs compile, the focused preset tests pass, and the
      complete core suite passes.

  **Implement:** `src/libslic3r/Preset.cpp` · `src/libslic3r/PresetBundle.cpp`
  **Verify:** `tests/libslic3r/test_preset_bundle_loading.cpp`

- [x] **Score 5 🥇 · Class 3 — Support interface material assignment regressed for multi-material supports (#13983)**
      — DONE verification (2026-08-24, per TRIAGE_POLICY.md's re-entry rule): GitHub shows this
      issue closed (`state_reason: completed`) at 2026-07-26T22:16:21Z, two seconds after
      OrcaSlicer/OrcaSlicer#14678 ("Support bugfixes", kisslorand) merged as `04e13200aa`.
      Confirmed an ancestor of this fork's current HEAD via `git merge-base --is-ancestor`
      (pulled in by the 2026-08-24 upstream sync) — touches `SupportCommon.cpp`,
      `SupportParameters.hpp`, `TreeSupport.cpp`, `TreeModelVolumes.cpp`,
      `TreeSupport3D.cpp`, `TreeSupportCommon.hpp`. No code change needed; closing as already
      shipped.

  **Context:** `src/libslic3r/Support/SupportMaterial.cpp` · `src/libslic3r/Support/TreeSupport.cpp`

- [x] **Score 3 🥈 · Class 3 — No fuzzy skin on bridges (#13891)** — Cherry-picked (2026-08-24,
      per `TRIAGE_POLICY.md`'s Cherry-pick tier) from OrcaSlicer/OrcaSlicer#13891 itself (open,
      mergeable, not a draft, single-file diff, fixes #9137). Root cause: `apply_fuzzy_skin`
      jittered the whole perimeter loop regardless of whether a segment was over solid material
      below or spanning a bridged void, degrading bridge surface quality/adhesion. Fix computes
      `expanded_lower_slices` (the layer below, grown by extrusion width + fuzzy thickness) and
      clips the fuzzy displacement to only the polyline/extrusion-line portions that intersect
      it, leaving bridge spans smooth — for both the classic (`Polygon`) and Arachne
      (`Arachne::ExtrusionLine`) code paths.
      **Reformatted on apply:** two blocks in the PR's raw diff had zero leading indentation
      (a formatting artifact, not intentional), reindented to match the surrounding 4-space
      style per `AGENTS.md`'s code-style note — no functional change.
      **Verified:** `fff_print_tests` builds clean; full suite and the `[Bridge]` tag (24 cases,
      101 assertions from the earlier #2231 cherry-pick) pass with this change present. No
      dedicated regression test exists for this fix (the upstream PR didn't add one either) —
      see the new coverage-gap finding below if picked up later.

  **Implement:** `src/libslic3r/Feature/FuzzySkin/FuzzySkin.cpp`

- [x] **Score 3 🥈 · Class 3 — Concentric top/bottom layers not continuous, regression since 2.2.0 (#8383)**
      — DONE verification (2026-08-24, per TRIAGE_POLICY.md's re-entry rule): a maintainer
      confirmed this fixed in upstream `main` on 2025-07-30 ("I can confirm it has been fixed!"),
      well before this fork's baseline. The issue's final closing comment (2026-08-02, after an
      auto-stale bump) cross-references #14179 and #11682 as the relevant fill-ordering work —
      both already confirmed shipped in this fork per `docs/shipped.md` ("Top/bottom fill order
      control" as `c84d5c7943`; "Anisotropic surfaces + Separated Infills" as `bc6ffcfb31` /
      `c4ea86041b`). No specific commit to pin for *this* issue directly (fixed via general
      upstream fill-ordering work, not a standalone patch), but given the maintainer confirmation
      predates this fork's last sync by over a year, treating as shipped rather than re-deriving
      from scratch. No code change needed.

  **Context:** `src/libslic3r/Fill/FillConcentric.cpp` · `src/libslic3r/Fill/FillConcentricInternal.cpp`

- [x] **Score 3 🥈 · Class 3 — Extra perimeters on overhangs vs. bridging (#10565)** — Cherry-
      picked (2026-08-24, per `TRIAGE_POLICY.md`'s Cherry-pick tier) from OrcaSlicer/OrcaSlicer#10565
      itself (open; `mergeable: CONFLICTING` against upstream `main`, but applied cleanly to this
      fork with one small conflict — see below). Complements, doesn't duplicate, the #2231 bridge
      cherry-pick (`erBridgePerimeter` role) already shipped: that changes G-code speed/flow/fan
      for perimeters already classified as bridge; this changes the *upstream* decision of
      whether `generate_extra_perimeters_over_overhangs` treats an overhang region as bridgeable
      at all — different stage of the same pipeline. Root cause: the old "extra perimeters on
      overhangs" heuristic forced extra-perimeter treatment whenever unbridgeable area exceeded
      20%, causing areas that should bridge to become overhangs instead. Fix redefines
      bridgeable as "has any anchored area AND no upper-layer holes over it" (replacing the
      percentage threshold), tightens the anchor margin constant (`EXTERNAL_INFILL_MARGIN` →
      `BRIDGE_INFILL_MARGIN`), and fixes a closed-loop anchor-detection edge case where a first
      overhang polyline exactly aligned with the layer below was wrongly treated as anchored
      (now shrunk by a quarter extrusion width before the intersection test).
      **Conflict resolved:** one 3-way merge conflict in `PerimeterGenerator.cpp` — the PR's base
      predates this fork's `Polyline`/`ArcPolyline` API split, so `overhang_region.front().polyline`
      needs an explicit `.to_polyline()` (matching the surrounding code's existing pattern);
      resolved by using the PR's new expression with that conversion added, and hit the same
      drift a second time at compile time (`to_polygons({overhang_region.front().polyline})` also
      needed `.to_polyline()`) — fixed post-conflict-resolution when the build caught it.
      **Verified:** `fff_print_tests` builds clean; full suite excluding the pre-existing
      `[Invalidation]` crash (see #13891's shipped entry) shows the same 3 pre-existing
      `test_support_material.cpp` failures as baseline; the `[Bridge]` tag (24 cases, 101
      assertions) passes unchanged, confirming no interaction with the #2231 cherry-pick.

  **Context:** `src/libslic3r/PerimeterGenerator.cpp`

- [x] **Score 3 🥈 · Class 3 — New wall order: Odd-Even (#10622)** — Cherry-picked (2026-08-24,
      per `TRIAGE_POLICY.md`'s Cherry-pick tier) from OrcaSlicer/OrcaSlicer#10622 itself (open,
      mergeable). Adds a new `WallSequence::OddEven` mode: odd-numbered inner walls print first
      (outer-to-inner), then even-numbered walls print in reverse (inner-to-outer), filling the
      grooves left between the odd walls with extra material for a more monolithic wall - with a
      configurable `loop_sequence` (inward/outward direction), `even_loops_flow_ratio` and
      `even_loops_speed` for the even pass, and `outermost_wall_control` to choose whether the
      outer wall is still isolated and printed last. Confirmed additive and backward-compatible
      before applying: new enum values (`WallInfillOrder::OddEvenInfill`, `WallSequence::OddEven`)
      are appended after all existing values and matched by string key (not ordinal), all new
      logic is gated behind `wall_sequence == WallSequence::OddEven` so existing wall sequences
      are untouched, and the four new config options are new keys with no collisions.
      **Cleaned up on apply** (not present in the shipped diff): dropped an unused
      `static WallSequence _wall_sequence_trig` variable in `ConfigManipulation.cpp` (declared,
      never read) per `AGENTS.md`'s "keep code concise" review focus, and stripped trailing
      whitespace the PR's raw diff introduced on ~10 lines across 6 files (comments and string
      literals only - no textual/behavioral change).
      **Verified:** `fff_print_tests` builds clean; full suite (excluding the pre-existing
      `[Invalidation]` crash) shows the same 3 pre-existing `test_support_material.cpp` failures
      as baseline; `[Bridge]` tag (24 cases) passes unchanged.

  **Implement:** `src/libslic3r/Arachne/utils/ExtrusionLine.hpp` · `src/libslic3r/Config.cpp` ·
  `src/libslic3r/ExtrusionEntity.cpp` · `src/libslic3r/ExtrusionEntity.hpp` ·
  `src/libslic3r/GCode.cpp` · `src/libslic3r/Layer.cpp` · `src/libslic3r/PerimeterGenerator.cpp` ·
  `src/libslic3r/Preset.cpp` · `src/libslic3r/Print.cpp` · `src/libslic3r/PrintConfig.cpp` ·
  `src/libslic3r/PrintConfig.hpp` · `src/slic3r/GUI/ConfigManipulation.cpp` ·
  `src/slic3r/GUI/Plater.cpp` · `src/slic3r/GUI/Tab.cpp`

- [x] **Score 3 🥈 · Class 3 — Port FillScatteredRectilinear from SuperSlicer (#12000)** —
      Cherry-picked (2026-08-24, per `TRIAGE_POLICY.md`'s Cherry-pick tier) from
      OrcaSlicer/OrcaSlicer#12000 itself (open, mergeable, 137+/-2 across 6 files). Competitor-
      port check per the antipattern: SuperSlicer is AGPLv3, same lineage as OrcaSlicer,
      license-compatible - confirmed, not just assumed. New `FillScatteredRectilinear` infill
      pattern (subclass of `FillRectilinear`) generates 100%-density vertical lines then randomly
      prunes them (seeded by layer index, via `boost::random::mt19937`) to hit the target density
      on average, giving a randomized light-diffusing texture instead of a uniform grid - useful
      for light panels/diffusers. Purely additive: new `InfillPattern::ipScatteredRectilinear`
      enum value appended before the `ipCount` sentinel (preserves existing ordinal positions,
      and matched by string key via `s_keys_map_InfillPattern` regardless), new subclass with no
      changes to `FillRectilinear`'s existing behavior, new icon resource. Stripped trailing
      whitespace on 4 lines the PR's raw diff introduced (no textual change) on apply.
      **Verified:** `fff_print_tests` builds clean; full suite (excluding the pre-existing
      `[Invalidation]` crash) shows the same 3 pre-existing `test_support_material.cpp` failures
      as baseline.

  **Implement:** `src/libslic3r/Fill/FillBase.cpp` · `src/libslic3r/Fill/FillRectilinear.cpp` ·
  `src/libslic3r/Fill/FillRectilinear.hpp` · `src/libslic3r/PrintConfig.cpp` ·
  `src/libslic3r/PrintConfig.hpp` · `resources/images/param_scatteredrectilinear.svg`

- [x] **Score 3 🥈 · Class 3 — Option to restrict Support/Raft Interface filament to the top layer only (#13109)**
      — Cherry-picked (2026-08-24, per `TRIAGE_POLICY.md`'s Cherry-pick tier) from the issue's
      own linked PR, OrcaSlicer/OrcaSlicer#14232 ("Option for Support Interface Filament only
      where necessary", open, mergeable, 72+/-5 across 11 files). New `minimal_support_interface`
      option: when enabled, support interface material is used only where it directly touches
      the printed object/raft (top/bottom/raft contact layers) - support built up *on top of
      already-printed support* (not touching the object) reverts to the regular support material
      instead, reducing filament changes for multiplexer (AMS-like) systems and helping with
      interface filaments that bond poorly to the support filament. Implemented via a new pure-
      virtual `ExtrusionEntity::set_extrusion_role_all()` (confirmed only 4 subclasses exist in
      this tree - `ExtrusionPath`, `ExtrusionMultiPath`, `ExtrusionLoop`,
      `ExtrusionEntityCollection` - all patched by the PR, so no missing-override compile risk)
      plus a new `PrintObject::reassign_on_object_support()` pass that walks support layers
      bottom-up and reassigns any support extrusion whose bounding box doesn't overlap the layer
      below to the plain support role.
      **Dropped one line on apply** (not in the shipped diff): the PR's
      `toggle_print_sla_options()` change (`toggle_field("support_minimal_support_interface", ...)`)
      references a config key that doesn't exist anywhere in this codebase (the real option is
      `minimal_support_interface`, FFF-only, no `support_` prefix) inside the *SLA* options
      toggle function - a copy-paste mistake in the PR, not something SLA actually has. Left out;
      `toggle_field` no-ops gracefully on a missing key either way, so this was inert, not unsafe,
      but not worth carrying forward.
      **Verified:** `fff_print_tests` builds clean; full suite (excluding the pre-existing
      `[Invalidation]` crash) and the `[SupportMaterial]` tag show only the same 3/1 pre-existing
      failures as baseline (the stale `discarded_branch_meshes` constant from #13983's shipped
      entry) - no new regressions.

  **Implement:** `src/libslic3r/ExtrusionEntity.hpp` · `src/libslic3r/ExtrusionEntityCollection.hpp` ·
  `src/libslic3r/Preset.cpp` · `src/libslic3r/Print.hpp` · `src/libslic3r/PrintConfig.cpp` ·
  `src/libslic3r/PrintConfig.hpp` · `src/libslic3r/PrintObject.cpp` ·
  `src/libslic3r/Support/SupportCommon.cpp` · `src/slic3r/GUI/ConfigManipulation.cpp` ·
  `src/slic3r/GUI/GUI_Factories.cpp` · `src/slic3r/GUI/Tab.cpp`

- [x] **Score 3 🥈 · Class 3 — Add a slicing tolerance option (#9801)** — Cherry-picked
      (2026-08-24, per `TRIAGE_POLICY.md`'s Cherry-pick tier) from the issue's own PR,
      OrcaSlicer/OrcaSlicer#9801 itself (open, not draft, 88+/-2 across 7 files). New
      `slicing_tolerance` option (Middle/Exclusive/Inclusive): for sloped walls, slices each
      layer's top and bottom Z boundary separately and combines them with an intersection
      (Exclusive - keeps extrusions strictly within the model, reduces volume) or a union
      (Inclusive - ensures full coverage, increases volume, useful before sanding/finishing)
      instead of the single mid-layer slice used today (Middle, the default and only behavior
      before this change). Gated behind `tolerance != SlicingTol::Default`, so the default
      "Middle" setting takes the exact original code path unchanged - purely additive.
      **Fixed on apply:** a real conflict in `Preset.cpp` (the PR's base predates this fork's
      reformatting of `s_Preset_print_options` from packed comma-lines to one-key-per-line, and
      several keys added by other cherry-picks already landed this session were on the "theirs"
      side) - resolved by inserting `"slicing_tolerance"` into the current one-per-line list
      rather than reintroducing the old packed format. Also fixed a `category = L("other")` typo
      (should be `L("Other")`, matching every other use of that category name in this file) and
      stripped trailing whitespace/a stray blank line the PR's raw diff introduced.
      **Verified:** `fff_print_tests` builds clean; full suite (excluding the pre-existing
      `[Invalidation]` crash) shows only the same 3 pre-existing failures as baseline.

  **Implement:** `src/libslic3r/Preset.cpp` · `src/libslic3r/PrintConfig.cpp` ·
  `src/libslic3r/PrintConfig.hpp` · `src/libslic3r/PrintObjectSlice.cpp` ·
  `src/libslic3r/TriangleMeshSlicer.cpp` · `src/libslic3r/TriangleMeshSlicer.hpp` ·
  `src/slic3r/GUI/Tab.cpp`

- [x] **Score 3 🥈 · Class 3 — Zero flow ironing (#10350)** — Cherry-picked (2026-08-24, per
      `TRIAGE_POLICY.md`'s Cherry-pick tier) from the issue's own PR, OrcaSlicer/OrcaSlicer#10350
      itself (open, not draft, mergeable, 73+/-19 across 9 files). Allows `ironing_flow = 0` -
      the nozzle passes over the top surface without extruding any material, purely for thermal
      smoothing. Adds `ironing_retract`/`ironing_unretract_extra` options so the filament is
      retracted before and restored after the zero-flow pass (avoiding oozing during a pass with
      no planned extrusion), and marks these zero-E ironing moves as `Extrude` type in
      `GCodeProcessor` (rather than travel) so the G-code preview shows them correctly. Refactors
      `GCodeWriter::_retract`/`unretract`'s bodies into new reusable `emit_retract`/
      `emit_unretract` helpers - confirmed behaviorally identical for existing callers (same E
      position, speed, comment computation, just extracted). New options are only consulted when
      `ironing_flow == 0`, so default (non-zero-flow) ironing is unaffected.
      **Verified:** `fff_print_tests` builds clean; full suite (excluding the pre-existing
      `[Invalidation]` crash) shows only the same 3 pre-existing failures as baseline.

  **Implement:** `src/libslic3r/GCode.cpp` · `src/libslic3r/GCode/GCodeProcessor.cpp` ·
  `src/libslic3r/GCodeWriter.cpp` · `src/libslic3r/GCodeWriter.hpp` · `src/libslic3r/Preset.cpp` ·
  `src/libslic3r/PrintConfig.cpp` · `src/libslic3r/PrintConfig.hpp` ·
  `src/slic3r/GUI/ConfigManipulation.cpp` · `src/slic3r/GUI/Tab.cpp`

- [x] **Score 3 🥈 · Class 3 — Variable fan speed (#6738)** — widely-requested cooling feature,
      touches the cooling buffer and emitted fan values.
      **Cherry-picked from PR #6738 (2026-08-25), with 5 bugs found and fixed during review** —
      the 2026-08-24 flag below caught real issues; on user approval, fixed all of them rather
      than deferring:
      1. **Sentinel-clamping bug (the one flagged below).** In `GCode.cpp`, the `-1` sentinel
         meaning "use `bridge_fan_speed` directly" was computed as
         `std::max(0, int(100 - overlap*100))`, which clamps the sentinel to `0` before the
         negative-check in `CoolingBuffer.cpp` ever sees it. Fixed by branching explicitly:
         `if (is_bridge(path.role()) && path.role() != erOverhangPerimeter) overhang_degree = -1;`
         before the `std::max` computation runs at all, so true bridges always reach the sentinel
         path.
      2. **`bridge_fan_speed` default-fallback regression (the one flagged below).** Unset
         (`-1`) `bridge_fan_speed` fell back to `overhang_fan_speed_max` instead of the user's
         existing `overhang_fan_speed` — exactly the silent-regression risk identified at
         pickup. Fixed the fallback in `CoolingBuffer.cpp` to use `overhang_fan_speed`.
      3. **`internal_bridge_fan_speed` default-fallback regression.** The PR also regressed this
         *pre-existing* option's fallback to `overhang_fan_speed_max`, directly contradicting its
         own "backward compatibility" comment. Fixed to fall back to `overhang_fan_speed`.
      4. **Double-scaling bug in the ramp-up branch.** The first-layers fan ramp reassigns
         `overhang_fan_speed` before scaling `overhang_fan_speed_max`, so the max was being
         scaled from an already-ramped value instead of its own pre-ramp value. Reordered so each
         value scales from its own original value.
      5. **Incomplete anchored ramp branch.** The "Option-B anchored ramp" branch only ramped
         `fan_speed_new`/`overhang_fan_speed`, leaving `overhang_fan_speed_max` and
         `bridge_fan_speed` unramped — a sudden full-speed fan burst on early layers once overhang
         degree crossed the threshold. Extended the branch to ramp all four values consistently.

      New options: `overhang_fan_speed_max`, `bridge_fan_speed` (independent bridge-cooling knob,
      default `-1` = fall back to `overhang_fan_speed`). Fan speed is now a gradient between
      `overhang_fan_speed` and `overhang_fan_speed_max` keyed on per-segment overhang degree, with
      true bridges using `bridge_fan_speed` directly via the sentinel path above.

      **Verified:** `fff_print_tests` builds clean on `build-vs2026`. Added two regression tests
      in `tests/fff_print/test_cooling.cpp` with hand-verified exact `M106 S<n>` PWM values —
      "Bridge fan speed is honored independently of the overhang fan speed" (S43) and "Unset
      bridge fan speed falls back to the overhang fan speed, not its maximum" (S94) — both pass.
      Full suite: 160/163 passing, with only the same 3 pre-existing unrelated failures as
      baseline (no regressions introduced).

  **Context:** `src/libslic3r/GCode/CoolingBuffer.cpp` · `src/libslic3r/GCode.cpp`
