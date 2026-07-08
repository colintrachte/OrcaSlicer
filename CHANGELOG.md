# Changelog

## [Unreleased] — 2026-07-03

### Features

- **Stagger perimeters ("brick layers" / vertical interlock walls)** (#7282)  
  New experimental print-region option `stagger_perimeters` (plus `stagger_perimeters_extrusion_multiplier`, default 1.05) alternates the Z-height of inner-wall perimeter rings by layer parity, so vertical seams in adjacent layers offset like bricks in a wall instead of stacking straight up — intended to improve z-strength. Implemented directly in the Arachne wall pipeline (`PerimeterGenerator::traverse_extrusions()`) rather than as a G-code post-processing script, reusing the existing ZAA (`z_contoured`) per-path Z-offset infrastructure. Only inner walls (`inset_idx > 0`) on layers past the first two are eligible, and a new slope/top-surface guard (comparing against `upper_slices`, mirroring the existing `detect_steep_overhang()` check against `lower_slices`) skips staggering any wall segment not fully covered by the layer above — fixing a documented failure mode ("staggers even when visible from above") in the prior community attempt at this feature (PR #8181). Arachne-only; mutually exclusive with Z contouring, spiral vase, and the Inner/Outer/Inner wall sequence, enforced via new `ConfigManipulation.cpp` auto-fix dialogs. Off by default. Not compile-verified in this checkout (no build environment available) — manual code review only.  
  Files: `src/libslic3r/PerimeterGenerator.cpp`, `src/libslic3r/PerimeterGenerator.hpp`, `src/libslic3r/GCode.cpp`, `src/libslic3r/ExtrusionEntity.hpp`, `src/libslic3r/ExtrusionEntity.cpp`, `src/libslic3r/PrintConfig.hpp`, `src/libslic3r/PrintConfig.cpp`, `src/libslic3r/Preset.cpp`, `src/slic3r/GUI/Tab.cpp`, `src/slic3r/GUI/ConfigManipulation.cpp`

### Bug Fixes

- **Preset load errors silently deleted the user's custom preset file** (#13075)  
  In `PresetCollection::load_presets()`, a JSON parse failure, an I/O error reading the file, or a runtime error while merging a user preset's diff against its parent (e.g. a vector-length mismatch after a vendor bundle update changes a `machine_max_*`/Motion Ability array's shape) each caught the error, logged it, and then deleted the preset's `.json` and `.info` files from disk before moving on. Any of these three error paths permanently destroyed the user's only copy of their custom Start/End G-code and Motion Ability settings, which then appeared to "silently revert to defaults" on the next launch because the file was gone. All three paths now just skip loading the affected preset and leave the file in place, so a transient error no longer costs the user their settings.  
  File: `src/libslic3r/Preset.cpp`

- **Added instances after the first stack on top of existing objects** (owner pain point)  
  `Plater::increase_instances()` placed each new instance copy at a fixed diagonal offset from the previous one, with no collision check against other objects already on the plate. The first added copy usually looked fine (landing on empty bed space), but every following copy — or any copy added when the plate wasn't empty — advanced by the same fixed step regardless of what was already there, landing on top of existing geometry instead of an empty spot. Now each new instance is placed via `GLCanvas3D::get_nearest_empty_cell()`, the same collision-aware placement `load_model_objects()` and the object-list copy path already use.  
  File: `src/slic3r/GUI/Plater.cpp`

- **Klipper Device tab crash when WebKit2GTK is unavailable** (#10756)  
  On Linux, if `libwebkit2gtk` is not installed or fails to initialize, `PrinterWebView::m_browser` is null. Four call sites (`Show()`, `reload()`, `update_mode()`, `SendAPIKey()`) dereferenced this null pointer, causing a segfault when switching to the Device tab. All are now null-guarded. The panel now also displays a user-readable message instructing users to install the missing library.  
  File: `src/slic3r/GUI/PrinterWebView.cpp`

- **Wipe tower extruder override (`wipe_tower_filament`) not respected** (#10971)  
  `WipeTower::get_wall_filament_for_all_layer()` used a layer-frequency heuristic to pick the wall extruder, ignoring the user's explicit `wipe_tower_filament` setting entirely. The per-layer lambda had an additional category-substitution fallback. Both now short-circuit when `wipe_tower_filament > 0`, so the user-specified extruder is always used for wipe tower walls.  
  Files: `src/libslic3r/GCode/WipeTower.cpp`, `src/libslic3r/GCode/WipeTower.hpp`

- **Travel acceleration cap incorrectly applied for Klipper** (#8582)  
  For Klipper printers, `SET_VELOCITY_LIMIT ACCEL=` was capped against `machine_max_acceleration_extruding` even for travel moves, ignoring the (usually higher) `machine_max_acceleration_travel` limit. Travel moves now use the correct travel cap (`machine_max_acceleration_travel`, optionally clamped by per-axis X/Y limits).  
  Files: `src/libslic3r/GCodeWriter.cpp`, `src/libslic3r/GCodeWriter.hpp`, `src/libslic3r/GCode.cpp`

- **Unused dead variable removed in GLCanvas3D**  
  `filament_id` (renamed from `filamnet_id`) was declared but never referenced in the filament-map loop. Variable removed.  
  File: `src/slic3r/GUI/GLCanvas3D.cpp`

### Dependencies

All four dependency upgrades update the version URL and SHA256 hash in the corresponding `deps/` cmake file. No source-level API changes are required for TBB or Boost. OpenSSL 3.x deprecates several low-level APIs used in OrcaSlicer; the deprecated calls are addressed as follows:

- **OpenSSL 1.1.1w → 3.4.6** (LTS, supported until Oct 2028) — EOL since Sept 2023.  
  - `MD5_CTX`/`MD5_Init`/`MD5_Update`/`MD5_Final` in `bbs_3mf.cpp` and `utils.cpp` migrated to `EVP_DigestInit_ex` / `EVP_DigestUpdate` / `EVP_DigestFinal_ex`.  
  - `HMAC()` one-shot and `SHA256()` one-shot (deprecated in 3.x, still available) suppressed via `OPENSSL_SUPPRESS_DEPRECATED` in `GUI_App.cpp` and `OrcaCloudServiceAgent.cpp`.  
  - `no-dynamic-engine` configure flag (removed in 3.x) replaced with `no-engine`.  
  - Redundant `#include <openssl/md5.h>` removed from `CreatePresetsDialog.cpp`.  
  Files: `deps/OpenSSL/OpenSSL.cmake`, `src/libslic3r/Utils.hpp`, `src/libslic3r/utils.cpp`, `src/libslic3r/Format/bbs_3mf.cpp`, `src/slic3r/GUI/GUI_App.cpp`, `src/slic3r/GUI/CreatePresetsDialog.cpp`, `src/slic3r/Utils/OrcaCloudServiceAgent.cpp`

- **CURL 7.75.0 → 8.20.0** — over 5 years of CVE fixes.  
  File: `deps/CURL/CURL.cmake`

- **TBB 2021.5.0 → 2021.13.0** — backward-compatible API.  
  File: `deps/TBB/TBB.cmake`

- **Boost 1.84.0 → 1.88.0** — uses the `-cmake` release tarball.  
  File: `deps/Boost/Boost.cmake`

### Code Quality

- **Spelling corrections across 49 source files**  
  Fixed pervasive misspellings in comments and variable/function names:
  - `filamnet` → `filament` (7 variable occurrences in 5 files)
  - `seperate_merged_filaments` → `separate_merged_filaments` (private method rename)
  - `m_is_bbl_filamnet` → `m_is_bbl_filament` (member variable rename)
  - `filamnet_preset_names` → `filament_preset_names`
  - `full_filamnet_serial` → `full_filament_serial`
  - `check_wipe_tower_existance` → `check_wipe_tower_existence` (parameter name)
  - `sucessful_preset` → `successful_preset` (loop variable)
  - `neccessary`/`unneccessary` → `necessary`/`unnecessary` (15 + 3 comment occurrences)
  - `seperate`/`seperately` → `separate`/`separately` (26 comment occurrences)
  - `accomodate` → `accommodate` (3 comment occurrences)
  - `occured` → `occurred` (1 comment occurrence)
  - Duplicate-word fixes: "is is", "of of", "to to", "in in" (10 comment occurrences)
  - `std::cerr` in `GCode.cpp` replaced with `BOOST_LOG_TRIVIAL(warning)`

- **HTTP read failure now logged**  
  The CURL read callback in `Http.cpp` was silently discarding exceptions. The exception message is now logged at error level before aborting the transfer.  
  File: `src/slic3r/Utils/Http.cpp`

### New Files

- **`run.sh`** — Linux/macOS developer run script.
- **`run.bat`** / **`run.ps1`** — Windows developer run scripts.
- **`TODO.md`** — Prioritized backlog derived from open GitHub issues and PRs.
- **`APPROVALS.md`** — Deferred items requiring design review before implementation.

---

## Previous Releases

See [GitHub Releases](https://github.com/OrcaSlicer/OrcaSlicer/releases) for historical release notes.
