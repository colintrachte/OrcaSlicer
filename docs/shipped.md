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
