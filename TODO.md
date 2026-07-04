# OrcaSlicer — Prioritized TODO

Generated: 2026-06-22 | Branch: main | Version: 2.5.0-dev (post v2.4.0 release 2026-06-20)

Sources: GitHub issues (open, sorted by reactions/comments), open PRs, codebase analysis.

---

## Critical — crashes, data loss

- [ ] **#10756** Crash when switching to Device tab for Klipper printers  
  WebView/Klipper UI tab crash on Linux (flatpak, AppImage, NixOS). Reported on 2.3.1+, still open.  
  `src/slic3r/GUI/` — Device tab / WebView widget.

- [ ] **#10524** Crash on startup due to Pango/freetype2 (Linux, 10 +1, 49 cmt)  
  `src/slic3r/GUI/` — font/rendering initialization path.

- [ ] **#11641** Crash opening Preferences (8 +1, 11 cmt)  
  Investigate preferences dialog initialization.

---

## High — broken functionality, major bugs

- [ ] **#10971** Wipe tower extruder override not respected in multimaterial (13 +1, 16 cmt)  
  Setting a specific extruder for wipe tower walls has no effect; dissimilar materials are placed directly on each other, causing layer adhesion failure. PrusaSlicer handles this correctly.  
  `src/libslic3r/Print.cpp`, `src/libslic3r/WipeTower*.cpp`

- [ ] **#2231** Bridges sliced as overhang walls instead of using bridge flow/density (10 +1, 67 cmt)  
  Bridge perimeters not getting bridge flow and density applied; sliced as overhang. Regressed from upstream.  
  `src/libslic3r/Fill/`, `src/libslic3r/LayerRegion.cpp`

- [ ] **#12314** Previously working filaments now show as "incompatible" after profile changes (5 +1, 48 cmt)  
  Compatibility filtering regression — valid filament/printer combos being incorrectly rejected.

- [ ] **#8582** Travel acceleration incorrectly limited by extrusion acceleration (7 +1, 9 cmt)  
  Travel moves capped to extruding accel, causing slower travels than configured.  
  `src/libslic3r/GCode/` — acceleration logic.

- [ ] **#7428** Slowed-overhang setting disables arc fitting for the entire layer (5 +1, 12 cmt)  
  When slow-overhang is active, arc fitting is suppressed globally on that layer rather than just on overhang segments.

- [ ] **#6433 / #11849 / #10059** Wayland + NVIDIA: no 3D view / unresponsive UI (Linux, many comments)  
  Multiple Wayland/NVIDIA rendering reports. Needs detection + fallback or explicit XWayland hint.

- [ ] **#11276** No workspace rendering at all (some Linux configs, 6 +1)

- [ ] **#12902** AppImage has hard dependency on `libbz2.so.1.0` (14 +1, 7 cmt)  
  Ubuntu 24.04 ships `libbz2.so.1.0.8` only; symlink missing. Needs bundling or runtime detection.

- [ ] **#12918 / #11251 / #11809** AppImage/Flatpak runtime EOL or startup failures on Ubuntu 24.04  
  Flatpak uses EOL `org.gnome.Platform` branch; AppImage doesn't run on Ubuntu 24.04.

---

## Medium — quality-of-life improvements and missing features with high demand

- [ ] **#2955** Spoolman integration — request filament IDs (136 rxn, most requested)  
  Allow OrcaSlicer to query Spoolman API for filament data / spool tracking.

- [ ] **#7282** Vertical brick layer / interlock layer printing (76 rxn)  
  Alternate layer orientation for stronger parts.

- [ ] **#12376** Add Anycubic Kobra X printer profile (46 rxn)  
  `resources/profiles/` — new printer profile JSON.

- [ ] **#6976** Multiple dovetail cuts (45 rxn)  
  Extend dovetail/joint cut tool to support multiple cuts per object.

- [ ] **#7106** Different filament profiles for outer walls, inner walls, and infill (33 rxn)  
  Per-feature filament override within a single extruder setup.

- [ ] **#12401** Allow filament to override ALL settings, not only selected ones (19 rxn)

- [ ] **#6334** Add "Pause at layer" option in the bottom layer slider (19 rxn)  
  Complements existing "Pause at height" with a layer-index pause.

- [ ] **#3564** Binary G-code (`.bgcode`) support (19 rxn)  
  PrusaSlicer already supports `.bgcode`; add export/import path.

- [ ] **#8900** Resin-style tree supports optimized for FDM (19 rxn)

- [ ] **#5796** Custom "prepare time" metadata for Klipper (20 rxn)  
  Allow per-printer preparation time override so ETA estimates are accurate in Klipper.

- [ ] **#997** UI slow to respond when changing settings tabs (11 +1, 79 cmt)  
  Profile: tab-switching triggers expensive re-renders or re-validation.

- [ ] **#5315** Improve Z-coordinate display placement in the UI (22 rxn)

- [ ] **#6885** Belt printer support (34 rxn) — architectural; track for roadmap.

- [ ] **#5053** Non-planar slicing (27 rxn) — research item; track for roadmap.

---

## Low — cosmetic, minor, platform-specific

- [ ] **#5783** Windows on ARM build (26 rxn)  
  No ARM64 Windows build exists yet; CI/CD extension needed.

- [ ] **#6866** Connect to printer over USB (31 rxn) — protocol-level work.

- [ ] **#10034** `dark_color_mode` ignored in dev Flatpak (6 +1)  
  Dark mode flag not propagated in Flatpak sandbox.

- [ ] **#8720** Printables integration broken on Flatpak (1 +1)

---

## Open PRs worth reviewing (ready or near-ready)

| PR | Title | Status |
|----|-------|--------|
| #14292 | Plate toolbar improvements and fixes | Needs review |
| #14216 | Fix bridging perimeters | Directly addresses #2231 |
| #14333 | Refactor skirt/brim + bugfixes | Large; needs careful review |
| #12470 | Improve thick bridges spacing | Related to #2231 |
| #13536 | New Boost library (1.91.0) | Dependency upgrade |
| #12724 | Custom filament profiles in MMU sync | Addresses #7106 area |
| #14145 | Nozzle size selector for non-BBL multi-extruder printers | Good QoL |
| #14179 | Top/bottom fill order control (Outward/Inward) | QoL |
| #11015 | Retract amount after wipe | QoL |
| #13459 | Small perimeters speed for supports | QoL |
| #13824 | Add 'brim layers' setting | QoL |
| #14324 | Gradient filament color preview | Visual QoL |
| #14340 | Minimal chamber temperature field | QoL |

---

## Infrastructure / CI

- [ ] **#12983** Build RPM workflow PR — evaluate and merge
- [ ] Flatpak runtime: update from EOL org.gnome.Platform to current LTS branch
- [ ] AppImage: bundle `libbz2` or ensure symlink compatibility on Ubuntu 24.04
- [ ] Add Windows ARM64 CI target (#5783)

---

## Notes

- v2.4.0 was just released 2026-06-20; trunk is now 2.5.0-dev
- PR #14216 (Fix bridging perimeters) and #12470 (Improve thick bridges spacing) directly target the long-standing bridge/overhang bug (#2231) — review these first
- The Klipper Device tab crash (#10756) has 135 comments; likely a WebView2/wxWebView crash on GTK; needs WebView null-guard or deferred initialization
- Many Wayland issues are fundamentally about missing EGL/Wayland backend selection; OrcaSlicer currently relies on GLX which conflicts with Wayland-native NVIDIA
