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
