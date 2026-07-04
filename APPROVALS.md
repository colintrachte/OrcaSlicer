# APPROVALS Queue

Items requiring design review or architectural decision before implementation.
Completed items are removed; see CHANGELOG.md for what was done.

---

## [BUG] Slowed Overhang Disables Arc Fitting for Overhang Paths (#7428)

**Issue:** When "Slow Down for Overhang" is active on any path, arc fitting (G2/G3) is bypassed for the ENTIRE path, even for non-overhang segments of the same extrusion loop.  
**Priority:** Medium — 5 +1, 12 comments.  
**Root cause:** In `GCode::_extrude` (`src/libslic3r/GCode.cpp`), when `variable_speed` is true (overhang slowing active), the code iterates through `new_points` and emits one G1 per point. The arc-fitting code path is only reached when `variable_speed` is false. Since `variable_speed` is set per-path (not per-segment), any path with even one overhang point loses arc fitting for the entire path.  
**Fix approach:** In the variable-speed loop, detect non-variable-speed arc segments from `path.polyline.fitting_result` and emit G2/G3 for those while keeping G1 for variable-speed overhang segments. Requires careful index tracking between `new_points` (per-pixel subdivided) and `fitting_result` (pre-overhang subdivision).  
**Action needed:** Design the mixed G1/G2/G3 variable-speed loop. Complex rewrite — needs design review and careful testing before implementation.

---

## [FEATURE/ARCH] Spoolman Integration (#2955)

**Request:** Connect to a Spoolman instance to look up filament IDs, track spool weights, and update remaining filament after a print.  
**Scope:** Significant feature — requires:
1. New printer/filament setting: `spoolman_url`
2. HTTP requests to Spoolman REST API (can reuse `src/slic3r/Utils/Http.cpp`)
3. UI: spool picker dialog in the filament panel
4. G-code post-processing hook to call Spoolman's `consume` API after print

**Architecture decision needed:** Integrate at the GUI layer (wxWidgets panel) or at the G-code export layer? The G-code export layer is more portable and does not depend on a running GUI.  
**Action needed:** Approve scope and integration layer before implementation begins.
