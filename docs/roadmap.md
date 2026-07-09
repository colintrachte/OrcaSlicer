# OrcaSlicer (fork) — Roadmap

Instantiated from [colintrachte/Project Management Tools](../../Project%20Management%20Tools)'s
`templates/ROADMAP.template.md`. Re-prioritized against this fork's own hardware (custom FDM
printers, Flashforge Adventurer Pro 5, future Prusa INDX) per `TRIAGE_POLICY.md`'s Hardware
Scope — not upstream SoftFever/OrcaSlicer's vote counts or issue labels.

`TODO.md` has been fully triaged into this file and deleted (2026-07-05) — this is now the
single source of truth for backlog items. New triage passes should add items directly here,
in this format, so `helper_scripts/route_tasks.py`/`pack_task.py` can route and pack them.
See the Notes section at the bottom for what was intentionally left out (already tracked
elsewhere, or excluded on hardware/vendor-path scope).

---

## Format convention

`helper_scripts/route_tasks.py` parses this file; `helper_scripts/context_pack.py` and
`helper_scripts/pack_task.py` consume the file lists you write into it. Keep new items
matching this shape:

- **Header line:** `- [ ] **Score <1-5> <medal> · Class <1-3> — <title>**`. `Class <n>`
  must appear in the header line itself, not just the body — see "Class (1–3)" below.
- **File-list fields** are their own line(s), starting with `  **Implement:**`,
  `  **Context:**`, `  **Write:**`, or `  **Read:**` (parenthetical suffixes like
  `**Implement (Class 3 — G-code exporter):**` are fine when one item spans tiers), each a
  `·`-separated list of backtick-quoted paths. Every item needs at least one such line — a
  routing tool with no file list can't route it.
- **`**Route:**`** is a generated annotation, not hand-authored — stamped by
  `route_tasks.py` per `docs/ai-harness.md` §2. Re-run the script after adding or editing
  items rather than filling it in by hand.
- **`**Effort:**`** is likewise generated, stamped right after `**Route:**` — a relative
  complexity score (`max(1, round(total_files * total_chars / 1000))`), not a time estimate.
- **`**Chars:**`** is likewise generated, stamped right after `**Effort:**` — total and
  largest-single-file character counts for the task's resolved file list. Flags up front
  when a task's largest file alone exceeds a model's inline-paste budget.
- When a todo item is marked complete `[x]`, move it to `shipped.md`.

## Scoring

### Score (1–5)

5 = highest priority. Ported items below got a mechanical carry-over from `TODO.md`'s
Critical/High/Medium/Low buckets (Critical→5, High→3 or 5 depending on whether it's a crash
vs. a workflow bug, Medium→3, Low→1) rather than a considered re-score — refine as each item
is actually picked up.

- **5** — crash, data loss, or a change to/around a Known Risky Subsystem
- **3** — real workflow bug or a widely-requested capability
- **1** — polish, or serves a deferred/secondary audience

The medal glyph is always mechanically derived, never hand-typed: `route_tasks.py`
re-derives it from the score on every run (≥4 → 🥇, =3 → 🥈, ≤2 → 🥉).

### Class (1–3)

OrcaSlicer has no separate `REVIEW_TIERS.md` — this maps directly onto `TRIAGE_POLICY.md`'s
**Known Risky Subsystems** (Arachne perimeter generation, support material, retraction/wipe
transitions, multi-tool sequencing/wipe tower, the G-code exporter):

- **Class 3** — touches a Known Risky Subsystem, or otherwise affects emitted G-code
  correctness. Never unattended; author review + a slice-and-inspect pass on a canonical
  test model before marking done (per `TRIAGE_POLICY.md`'s verification note).
- **Class 2** — settings/profile/UI logic that doesn't reach G-code emission. AI may
  propose and stage; a human approves before merge.
- **Class 1** — docs, cosmetic UI, packaging/CI, anything with no print-correctness risk.
  AI may merge if the build/tests pass and the change stays confined to this class.

When a change spans classes, it takes the bar of the highest class it touches.

### Effort (generated, not hand-typed)

```
effort = max(1, round(total_files * total_chars / 1000))
```

---

## How this roadmap is organized

By severity, carried over from `TODO.md`'s existing Critical → High → Medium → Low
buckets — see `TRIAGE_POLICY.md`'s categories, which map directly onto this.

---

## 1. Critical — crashes, data loss

- [ ] **Score 5 🥇 · Class 2 — Crash switching to Device tab for Klipper printers (#10756)**
  — WebView/Klipper UI tab crash on Linux (flatpak, AppImage, NixOS). Reported on 2.3.1+,
  still open, 135 comments. Likely a WebView2/wxWebView null-guard or deferred-init issue on
  GTK. Does not touch G-code emission — UI-layer crash.

  **Context:** `src/slic3r/GUI/PrinterWebView.cpp` · `src/slic3r/GUI/PrinterWebView.hpp` · `src/slic3r/GUI/Monitor.cpp`
  **Route:** gemini
  **Effort:** 90
  **Chars:** ~29,889 total (largest: src/slic3r/GUI/Monitor.cpp ~17,443) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 2 — Crash on startup due to Pango/freetype2 (#10524)** — Linux
  only, 10 +1, 49 comments. Font/rendering initialization path.

  **Context:** `src/slic3r/GUI/GUI_App.cpp` · `src/slic3r/GUI/GUI_App.hpp`
  **Route:** kimi
  **Effort:** 846
  **Chars:** ~422,809 total (largest: src/slic3r/GUI/GUI_App.cpp ~387,785) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 2 — Crash opening Preferences (#11641)** — 8 +1, 11 comments.
  Investigate preferences dialog initialization.

  **Implement:** `src/slic3r/GUI/Preferences.cpp`
  **Route:** gemini
  **Effort:** 96
  **Chars:** ~96,485 total (largest: src/slic3r/GUI/Preferences.cpp ~96,485) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 2 — Segfault on startup, Debian 13 / Arch AUR builds (#10434)**
  — X11 (not Wayland) crash before a printer can even be selected: "Cannot register URI
  scheme wxfs more than once" followed by SIGSEGV. Distinct signature from #10524
  (Pango/freetype2) despite both being crash-on-launch — no workaround found in-thread.

  **Context:** `src/slic3r/GUI/Widgets/WebView.cpp` · `src/slic3r/GUI/WebViewDialog.cpp`
  **Route:** gemini
  **Effort:** 99
  **Chars:** ~49,441 total (largest: src/slic3r/GUI/WebViewDialog.cpp ~31,934) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all
  (wxfs URI scheme registration — unverified, needs a diff-time confirmation pass)

- [ ] **Score 5 🥇 · Class 2 — Slicing crashes with small-area flow compensation on non-English UI (#12476)**
  — High-confidence root cause: `small_area_infill_flow_compensation_model` is stored as
  `coStrings` (numeric coefficients as text) and likely fails locale-sensitive
  `atof`/`strtod` parsing under a comma-decimal locale (repros on Russian, Spanish,
  Ukrainian; not English). Blocks slicing entirely for affected users.

  **Context:** `src/libslic3r/PrintConfig.cpp` (`small_area_infill_flow_compensation_model`, ~line 4427)
  **Route:** kimi
  **Effort:** 567
  **Chars:** ~566,682 total (largest: src/libslic3r/PrintConfig.cpp ~566,682) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

---

## 2. High — broken functionality, major bugs

- [ ] **Score 5 🥇 · Class 3 — Wipe tower extruder override not respected in multimaterial (#10971)**
  — Setting a specific extruder for wipe tower walls has no effect; dissimilar materials
  are placed directly on each other, causing layer adhesion failure. PrusaSlicer handles
  this correctly (regression relative to upstream behavior this fork also relies on).
  Touches the wipe tower — a Known Risky Subsystem (multi-tool sequencing).

  **Implement:** `src/libslic3r/Print.cpp` · `src/libslic3r/GCode/WipeTower.cpp` · `src/libslic3r/GCode/WipeTower2.cpp`
  **Context:** `src/libslic3r/GCode/ToolOrdering.cpp`
  **Route:** claude
  **Effort:** 2698
  **Chars:** ~674,391 total (largest: src/libslic3r/Print.cpp ~262,171) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 3 — Bridges sliced as overhang walls instead of using bridge flow/density (#2231)**
  — Bridge perimeters not getting bridge flow and density applied; sliced as overhang.
  Regressed from upstream. 67 comments — long-standing. Two open PRs target this directly:
  #14216 ("Fix bridging perimeters") and #12470 ("Improve thick bridges spacing") — review
  those first per `TRIAGE_POLICY.md`'s Cherry-pick tier before implementing from scratch.
  Affects emitted G-code (extrusion flow/density), so Class 3 even though `Fill/` isn't one
  of the five subsystems named by title in `TRIAGE_POLICY.md` — it fits the tie-breaker
  ("does it change a slicer calculation that ends up in emitted G-code values?").

  **Implement:** `src/libslic3r/Fill/` · `src/libslic3r/LayerRegion.cpp`
  **Route:** claude
  **Effort:** 105
  **Chars:** ~52,671 total (largest: src/libslic3r/LayerRegion.cpp ~52,671) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Previously working filaments now show as "incompatible" after profile changes (#12314)**
  — Compatibility filtering regression; valid filament/printer combos incorrectly
  rejected. 48 comments. Settings/profile logic, not G-code emission.

  **Context:** `src/libslic3r/PrintConfig.cpp`
  **Route:** kimi
  **Effort:** 567
  **Chars:** ~566,682 total (largest: src/libslic3r/PrintConfig.cpp ~566,682) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 2 — Preset management UX: opaque inheritance, silent duplication on
  import, and invisible compatibility rules (owner pain point, 2026-07-05; re-prioritized to
  High 2026-07-05)**
  — Owner-authored, not a single upstream issue: the data model (system → user preset chains
  via an `inherits` pointer, diff-only storage, printer/filament/process compatibility lists)
  is workable, but the UI surfaces almost none of it, and several real upstream bugs make the
  opacity actively harmful rather than just annoying. **Re-scored 3→5 and moved Medium→High**:
  this is recurring friction and occasional silent data loss hit on *every* preset save/load/
  import, on the owner's daily-driver platform (Windows) and owned hardware, versus several
  items still scored 5 in Critical/§1 above that are platform-gated to Linux distros/desktop
  environments this fork doesn't run (#10524 Pango/freetype2 crash, #10434 X11 segfault,
  #7649 en_GB locale crash) — a one-time crash on an unused platform is lower real-world
  priority than chronic friction plus confirmed silent data loss on the platform actually used.
  This is a broad initiative — **do not implement as one diff**; scope and decompose into
  independent sub-items (see candidate list below) before picking up any part, same as the
  PR-decomposition rule for bundles.

  **Confirmed root causes (read from this codebase, not speculation):**
  - **Paired `.json` + `.info` files**: each user preset is one `.json` (config values,
    written by `Preset::save()`, `src/libslic3r/Preset.cpp:634`) plus a sidecar `.info`
    (cloud-sync metadata: `sync_info`/`user_id`/`setting_id`/`base_id`, `Preset.cpp:586-609`).
    `remove_files()` (`Preset.cpp:611-631`) can delete the `.json` while deliberately keeping
    the `.info` (`sync_info="delete"`) pending cloud confirmation — a documented orphaned-file
    path. Copying/backing up a preset by hand means copying two files per preset; missing one
    silently breaks it.
  - **Diff-only storage + single-parent `inherits`**: a child preset's `.json` stores only the
    keys that differ from its parent (`config.diff(parent_preset->config)`,
    `Preset.cpp:1748-1790`); `inherits` (`Preset.hpp:308-311`) is a bare name string resolved
    by `PresetCollection::get_preset_parent()` (`Preset.cpp:3028-3050`). Nothing in the UI
    shows this chain, so a user has no way to tell which preset an edit actually lands in
    (base, an intermediate link, or a fork) without opening the JSON. Confirmed upstream:
    **#9536** ("Support inheritance from a user profile", 19 comments) — chaining a user
    preset's `inherits` onto *another user preset* (not a system preset) throws
    `can not find parent for config ...json!` and is rejected outright, so the "parent/child"
    model only reliably works one level below a system preset.
  - **Duplicate-name handling has no merge path**: on 3MF/project import
    (`PresetCollection::load_external_preset`, `Preset.cpp:2416`), a name collision with a
    differing config synthesizes `prefix + "(" + name + ")"` (`Preset.cpp:2605`), incrementing
    `-1`, `-2`, etc. — there is no literal `"(copy)"` string, but the effect matches the
    complaint: a second, differently-scoped copy is created silently, with no prompt to
    merge, replace, or rebind. Bundle/JSON import (`PresetBundle::import_json_presets`,
    `PresetBundle.cpp:1480-1491`) instead prompts yes/no/yes-to-all/no-to-all on collision and
    silently *drops* the import if declined — the opposite failure mode (data loss instead of
    duplication). Confirmed upstream: **#8216** ("Copy printer/filament/process profiles
    across printers in bulk") and the general shape matches multiple closed reports.
  - **Compatibility ("is this preset visible right now") is opaque and has shipped with a
    literal bug**: `Preset::is_visible` (`Preset.hpp:227`) is set by
    `set_visible_from_appconfig()` (`Preset.cpp:818-847`) from AppConfig printer-variant/
    filament-install markers; combo boxes just filter on it silently
    (`PresetComboBoxes.cpp:402-408`, `1179-1186`, `1692`, `2020`) with no "why isn't this
    here" affordance. Confirmed upstream: **#12193** ("Filament preset disappears after
    checking 'All printers' due to typo in JSON key ('compatible_prints' instead of
    'compatible_printers')") — Orca itself wrote the wrong key name into its own preset,
    silently hiding the preset with zero error surfaced. **#3497** ("Allow editing compatible
    printers... more easily", closed as completed) documents the export→hand-edit-JSON→
    reimport workaround users were reduced to before an in-app editor existed — worth
    re-verifying that editor actually covers all the cases users hit.
  - **User-preset JSON format differs from system-preset format, undocumented**: **#12223**
    ("User process presets: undocumented format, silent import failures, misleading errors",
    closed as duplicate — find and check the still-open canonical issue before starting) —
    hand-authoring or hand-editing a process preset from a copied system preset fails import
    with "You need to import the corresponding printer first" even when the printer is
    already configured, and the only way to discover the correct on-disk shape is reverse-
    engineering a UI-generated file.
  - **`sync_user_preset` (cloud sync) is a real, separate overwrite vector**: the "Auto sync
    user presets (Printer/Filament/Process)" checkbox (`Preferences.cpp:1712`,
    `start_sync_user_preset()`/`stop_sync_user_preset()`) pulls cloud state over local presets.
    Confirmed upstream data-loss reports plausibly tied to this path: **#14420** ("All process
    profiles missing after updating to 2.4.0+", 10 comments), **#14396** ("Not all process
    profiles synced to Orcacloud"), **#13967** ("Orca reset my config and I can't get it
    back"), **#14210** ("Show the user which preset has a conflict with Orca Cloud preset").

  **Already tracked elsewhere in this roadmap — don't duplicate, cross-reference instead:**
  #13075 (Critical §1, printer profiles silently losing custom G-code/Motion Ability settings
  — a persistence-layer instance of this same opacity), #12314 immediately above (compatibility
  filtering regression — a direct symptom of this item's `is_visible`/compatibility root cause),
  #14217 (§6 PR queue, preset-loading performance — may share root cause investigation with
  slow tab-switching #997), #13573 (§6 PR queue, sibling filament presets hidden when a generic
  is installed — an `is_visible` bug of the same family as #12193 above).

  **Comparison to PrusaSlicer / SuperSlicer (owner asked whether either solved this):** no
  evidence found of either project having actually solved it — same lineage, same diff-based
  single-parent `inherits` model. PrusaSlicer lacks Orca's Bambu-cloud-sync layer, so it avoids
  the `sync_user_preset` overwrite vector specifically, but has its own open reports of the
  same underlying pattern (e.g. prusa3d/PrusaSlicer **#3729** "Profile inheritance should check
  'conditions' on computed profile only", **#12626** "Provide Parent Preset Link Button" — a
  feature request for exactly the missing-visibility problem described here). SuperSlicer
  exposes a bit more of the inherits chain in its UI but is architecturally the same fork
  lineage pre-dating Orca. Treat "PrusaSlicer/SuperSlicer already solved this" as false — it's
  an unsolved problem across the whole fork family, not something to port from a sibling.

  **Candidate improvement directions (unscoped — needs its own design/triage pass, not a
  committed design):** an in-UI dependency view (parents/children/"referenced by N presets")
  instead of requiring JSON inspection; replacing silent `(name)`-suffix duplication on import
  with an explicit replace/merge/keep-both/rename prompt; a "why isn't this preset showing?"
  inspector surfacing the specific `is_visible`/`compatible_printers` reason (would have caught
  #12193's typo immediately instead of shipping it); fixing the known "detach" edge case
  (**#13057**, detaching a child that itself has children needs further manual steps);
  documenting the user-preset JSON schema so #12223's class of bug can't recur; and auditing
  whether `sync_user_preset` needs a conflict-resolution UI rather than last-write-wins.

  **Context:** `src/libslic3r/Preset.cpp` · `src/libslic3r/Preset.hpp` · `src/libslic3r/PresetBundle.cpp` · `src/slic3r/GUI/SavePresetDialog.cpp` · `src/slic3r/GUI/PresetComboBoxes.cpp` · `src/slic3r/GUI/ConfigWizard.cpp` · `src/slic3r/GUI/Preferences.cpp`
  **Route:** claude
  **Effort:** 6225
  **Chars:** ~889,339 total (largest: src/libslic3r/PresetBundle.cpp ~308,142) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Travel acceleration incorrectly limited by extrusion acceleration (#8582)**
  — Travel moves capped to extruding accel, causing slower-than-configured travels. Feeds
  directly into G-code acceleration values — Known Risky Subsystem (G-code exporter). Open
  PR **#12148** ("`travel_short_distance_acceleration`, ported from Bambu Studio") in the PR
  queue below touches the same acceleration path — verify BambuStudio source parity per
  `TRIAGE_POLICY.md` before treating it as AGPL-clean, then read the diff.

  **Context:** `src/libslic3r/GCode.cpp`
  **Route:** claude
  **Effort:** 451
  **Chars:** ~451,201 total (largest: src/libslic3r/GCode.cpp ~451,201) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 1 — AppImage hard dependency on `libbz2.so.1.0` (#12902)**
  — Ubuntu 24.04 ships `libbz2.so.1.0.8` only; symlink missing. Needs bundling or runtime
  detection. Packaging-only, no print-correctness risk.

  **Context:** `.github/workflows/build_orca.yml`
  **Route:** gemini
  **Effort:** 31
  **Chars:** ~30,683 total (largest: .github/workflows/build_orca.yml ~30,683) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 1 — AppImage missing `libmspack.so.0` (Ubuntu 24.04) (#12918)**
  — Same root-cause family as #12902 above (a shared lib the AppImage assumes is present
  isn't on stock Ubuntu 24.04) but a different library — needs its own bundling fix.

  **Context:** `.github/workflows/build_orca.yml`
  **Route:** gemini
  **Effort:** 31
  **Chars:** ~30,683 total (largest: .github/workflows/build_orca.yml ~30,683) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — "Create printer" fails to find some vendors (#5499)**
  — Confirmed across many vendors (Creality, Bambu, Prusa, etc.) — "model not found" blocks
  profile creation entirely for affected vendors. Labeled `stale` but the repro is broad and
  structural (profile lookup, not a one-off), not actually fixed.

  **Context:** `src/slic3r/GUI/CreatePresetsDialog.cpp`
  **Route:** kimi
  **Effort:** 265
  **Chars:** ~265,214 total (largest: src/slic3r/GUI/CreatePresetsDialog.cpp ~265,214) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Wall prints twice as thick with narrow line width, Classic perimeter mode (#10058)**
  — Confirmed root cause: when line width is narrower than the model wall, Classic-mode
  perimeter generation lays down two overlapping layers at the same height (workaround:
  switch to Arachne). Real print-quality/material-waste bug in wall generation.

  **Context:** `src/libslic3r/Fill/` (Classic perimeter path — exact function not yet
  **Route:** claude
  **Effort:** 1
  pinned down, confirm at diff-read time)

- [ ] **Score 3 🥈 · Class 2 — Window focus randomly steals between instances, can abort an active print (#9874)**
  — Verified real consequence: a reporter's active 15-hour print was stopped because focus
  silently stole to another instance mid-click. In-thread investigation traces the trigger
  to `show_status()`/`MONITOR_DISCONNECTED` calling `Raise()`. No workaround.

  **Context:** `src/slic3r/GUI/Widgets/SideTools.cpp`
  **Route:** gemini
  **Effort:** 21
  **Chars:** ~21,104 total (largest: src/slic3r/GUI/Widgets/SideTools.cpp ~21,104) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Start G-code uses non-blocking M104 instead of M109, print starts before hotend at temp (#4337)**
  — Confirmed on Voxelab/Anycubic and other non-Bambu printers: generated start G-code
  doesn't wait for target hotend temp before extruding. A manual M109 workaround exists in
  custom G-code, but the emitted-G-code root cause is directly fixable.

  **Context:** `src/libslic3r/GCode.cpp` (M104/M109 emission, ~lines 1232, 1324, 4047)
  **Route:** claude
  **Effort:** 451
  **Chars:** ~451,201 total (largest: src/libslic3r/GCode.cpp ~451,201) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 3 — Multimaterial (IFS) broken on Flashforge Adventurer 5X — owned hardware (#10783)**
  — Directly affects this fork's own Flashforge AD5X. Orca doesn't tell the printer which
  IFS slot to use, so the printer expects pre-loaded filament and throws false runout
  errors — blocks multimaterial printing on this exact printer. An open community PR
  (author marianomd) already implements a fix — read that diff first per the Cherry-pick
  tier before implementing from scratch.

  **Context:** `src/slic3r/Utils/Flashforge.cpp` · `src/slic3r/Utils/Flashforge.hpp`
  **Route:** claude
  **Effort:** 58
  **Chars:** ~29,202 total (largest: src/slic3r/Utils/Flashforge.cpp ~25,680) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Cannot connect to Elegoo Centauri Carbon 2 (#13550)**
  — Real Elegoo Link protocol handshake failure, not Bambu-exclusive — fits the
  print-host-support hardware scope. Open PR **#13212** already addresses this — read that
  diff first per the Cherry-pick tier.

  **Context:** `src/slic3r/Utils/` (no dedicated Elegoo file exists yet — this PR would add one)
  **Route:** gemini
  **Effort:** 1

- [ ] **Score 3 🥈 · Class 3 — Color-change sections force fan to 100%, ignoring filament profile min/max (#3677)**
  — Confirmed by a second reporter: filament-profile fan-speed bounds are ignored
  specifically during color-change sections, forcing 100% against e.g. a PETG profile's
  20-35% limit. Real, verifiable fan-speed (M106) emission bug.

  **Context:** `src/libslic3r/GCode.cpp` (fan-speed/M106 emission)
  **Route:** claude
  **Effort:** 451
  **Chars:** ~451,201 total (largest: src/libslic3r/GCode.cpp ~451,201) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Crash when system language is `en` without `en_GB` locale installed (#7649)**
  — Confirmed root cause: hardcoded fallback to `en_GB` instead of a generic `en`/`en_US`
  breaks on minimal Linux installs lacking that specific locale. A workaround exists
  (install en_GB), but this is locale-*detection* logic, not translation content — not the
  "touches localization strings" antipattern.

  **Context:** `src/slic3r/GUI/GUI_App.cpp` (wxLocale handling, ~lines 7290-7460)
  **Route:** kimi
  **Effort:** 388
  **Chars:** ~387,785 total (largest: src/slic3r/GUI/GUI_App.cpp ~387,785) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 2 — Blank / non-rendering viewport cluster on Linux (Nvidia, GTK) — investigate once (#11276, #10958)**
  — Both issues show the same GTK-CRITICAL blank-viewport signature reported independently
  across Pop!_OS, Fedora, and Ubuntu 25.10/Flatpak, and likely share a root cause with
  several Wayland-only reports skipped during this pass (#6433, #8145, #11698, #10059,
  #11434) and the Nvidia-driver-only #10090 (workaround there is a driver downgrade, outside
  Orca's control). Track as a single bounded Investigate-tier item — TRIAGE_POLICY.md's
  default scope (3 files or eliminate the main hypotheses) — rather than five duplicate
  entries; do not implement per-issue fixes until a shared cause is confirmed or ruled out.

  **Context:** GTK/Nvidia rendering init path — not yet localized to a specific file
  **Route:** gemini
  **Effort:** 1

---

## 3. Medium — quality-of-life improvements and missing features with high demand

- [ ] **Score 3 🥈 · Class 3 — Different filament profiles for outer walls, inner walls, and infill (#7106)**
  — Per-feature filament/extruder assignment within a single multi-material setup (33 rxn).
  Assigning which extruder prints which region changes tool-change ordering, so this touches
  multi-tool sequencing (Known Risky Subsystem). Open PR **#12724** ("Custom filament profiles
  in MMU sync") addresses this area — read that diff first per `TRIAGE_POLICY.md`'s
  Cherry-pick tier before implementing from scratch.

  **Implement:** `src/libslic3r/Print.cpp` · `src/libslic3r/GCode/ToolOrdering.cpp`
  **Context:** `src/libslic3r/PrintConfig.cpp`
  **Route:** claude
  **Effort:** 2746
  **Chars:** ~915,486 total (largest: src/libslic3r/PrintConfig.cpp ~566,682) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — UI slow to respond when changing settings tabs (#997)**
  — 79 comments; long-standing complaint. Root cause unknown (profile: tab-switching triggers
  expensive re-renders or re-validation) — Investigate + Implement tier per
  `TRIAGE_POLICY.md`: bound the investigation to 3 files or eliminating the main hypotheses,
  whichever comes first, before committing to a fix.

  **Context:** `src/slic3r/GUI/Tab.cpp`
  **Route:** kimi
  **Effort:** 428
  **Chars:** ~428,254 total (largest: src/slic3r/GUI/Tab.cpp ~428,254) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Connect to printer over USB (#6866)**
  — 31 rxn. Direct-serial print-host path; in scope per `TRIAGE_POLICY.md`'s hardware note
  that print-host upload behavior is always in scope, and genuinely useful for the Flashforge/
  Klipper setups this fork owns when WiFi upload isn't available. Effort is high (new
  transport/protocol layer, no existing serial code found) — Impact 2, Effort 3 nets below
  the take threshold on the formula alone, but demand and hardware relevance justify keeping
  it visible rather than dropping it; revisit when a wired workflow is actually needed.

  **Context:** `src/slic3r/Utils/OctoPrint.cpp` · `src/slic3r/Utils/Moonraker.cpp`
  **Route:** gemini
  **Effort:** 129
  **Chars:** ~64,295 total (largest: src/slic3r/Utils/OctoPrint.cpp ~49,873) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 3 — Resin-style tree supports optimized for FDM (#8900)**
  — 19 rxn. Touches Support material (Known Risky Subsystem) for a niche support style;
  low priority relative to the Critical/High support-adjacent work above.

  **Context:** `src/libslic3r/Support/TreeSupport.cpp` · `src/libslic3r/Support/TreeModelVolumes.cpp`
  **Route:** claude
  **Effort:** 495
  **Chars:** ~247,680 total (largest: src/libslic3r/Support/TreeSupport.cpp ~195,177) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 3 — Non-planar slicing (#5053)**
  — 27 rxn, research item. Would touch the G-code exporter and Arachne (Known Risky
  Subsystems) if ever attempted. No owned hardware currently exploits non-planar motion —
  re-check if/when hardware capable of it (5-axis, tilting bed, etc.) is added.

  **Context:** `src/libslic3r/GCode.cpp`
  **Route:** claude
  **Effort:** 451
  **Chars:** ~451,201 total (largest: src/libslic3r/GCode.cpp ~451,201) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 3 — Binary G-code (`.bgcode`) support (#3564)**
  — 19 rxn. PrusaSlicer already supports `.bgcode`. New export/import path in the G-code
  exporter itself (Known Risky Subsystem), so Class 3 despite modest demand.

  **Context:** `src/libslic3r/GCode.cpp`
  **Route:** claude
  **Effort:** 451
  **Chars:** ~451,201 total (largest: src/libslic3r/GCode.cpp ~451,201) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 2 — Add "Pause at layer" option (#6334)**
  — 19 rxn. Complements the existing "Pause at height" with a layer-index variant; a bounded
  extension of an already-working pattern.

  **Context:** `src/libslic3r/GCode.cpp`
  **Route:** kimi
  **Effort:** 451
  **Chars:** ~451,201 total (largest: src/libslic3r/GCode.cpp ~451,201) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 2 — Custom "prepare time" metadata for Klipper (#5796)**
  — 20 rxn. Per-printer preparation-time override so ETA estimates are accurate on Klipper —
  hardware-relevant (Klipper matters to the owned Flashforge Adventurer Pro 5) even though
  the feature itself is a small config + display change.

  **Context:** `src/libslic3r/PrintConfig.cpp` · `src/libslic3r/GCode.cpp`
  **Route:** kimi
  **Effort:** 2036
  **Chars:** ~1,017,883 total (largest: src/libslic3r/PrintConfig.cpp ~566,682) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 2 — Allow filament to override ALL settings, not only selected ones (#12401)**
  — 19 rxn. Config-scope widening; single-file change.

  **Context:** `src/libslic3r/PrintConfig.cpp`
  **Route:** kimi
  **Effort:** 567
  **Chars:** ~566,682 total (largest: src/libslic3r/PrintConfig.cpp ~566,682) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 2 — Multiple dovetail cuts (#6976)**
  — 45 rxn. Extend the existing cut-tool gizmo to support multiple cuts per object.

  **Context:** `src/slic3r/GUI/Gizmos/GLGizmoCut.cpp`
  **Route:** kimi
  **Effort:** 173
  **Chars:** ~173,372 total (largest: src/slic3r/GUI/Gizmos/GLGizmoCut.cpp ~173,372) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 1 — Improve Z-coordinate display placement in the UI (#5315)**
  — 22 rxn. Pure UI positioning, no slicing-logic risk.

  **Context:** `src/slic3r/GUI/IMSlider.cpp`
  **Route:** gemini
  **Effort:** 81
  **Chars:** ~81,006 total (largest: src/slic3r/GUI/IMSlider.cpp ~81,006) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Concentric top/bottom layers not continuous, regression since 2.2.0 (#8383)**
  — Confirmed still present in 2.3.0/2.3.1. Reporter traces it to a seam-staggering
  interaction with concentric fill ordering; affects surface quality broadly. Touches fill
  path ordering that reaches emitted G-code.

  **Context:** `src/libslic3r/Fill/FillConcentric.cpp` · `src/libslic3r/Fill/FillConcentricInternal.cpp`
  **Route:** claude
  **Effort:** 22
  **Chars:** ~11,064 total (largest: src/libslic3r/Fill/FillConcentric.cpp ~6,408) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Organic tree supports print in air, first ~6 layers missing (#4158)**
  — Acknowledged by a maintainer as inherited from upstream PrusaSlicer's organic
  tree-support implementation (also present in BambuStudio, still unfixed there). Real
  print-failure risk (unsupported floating trees) on any printer using organic supports.

  **Context:** `src/libslic3r/Support/TreeSupport3D.cpp` · `src/libslic3r/Support/TreeModelVolumes.cpp`
  **Route:** claude
  **Effort:** 602
  **Chars:** ~300,776 total (largest: src/libslic3r/Support/TreeSupport3D.cpp ~248,273) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 3 — Support interface material assignment regressed for multi-material supports (#13983)**
  — Filed as an enhancement but is a regression: up to 2.3.2 the first support-interface
  layer printed in the support material and subsequent layers in the model material; that
  assignment broke, causing poor interface adhesion. Touches Support material (Known Risky
  Subsystem) — a real, if non-crashing, print-quality regression, not a feature request.

  **Context:** `src/libslic3r/Support/SupportMaterial.cpp` · `src/libslic3r/Support/TreeSupport.cpp`
  **Route:** claude
  **Effort:** 746
  **Chars:** ~373,052 total (largest: src/libslic3r/Support/TreeSupport.cpp ~195,177) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Option to restrict Support/Raft Interface filament to the top layer only (#13109)**
  — Currently the interface material applies to every interface layer, not just the one
  touching the model. Real workflow request (22 rxn) for multi-material support setups.
  Touches Support material (Known Risky Subsystem).

  **Context:** `src/libslic3r/Support/SupportMaterial.cpp`
  **Route:** claude
  **Effort:** 178
  **Chars:** ~177,875 total (largest: src/libslic3r/Support/SupportMaterial.cpp ~177,875) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Variable line width on overhangs (#12029)**
  — Wider line width on overhang perimeters measurably improves printability of otherwise
  impossible overhangs. Touches wall generation (Arachne-adjacent).

  **Context:** `src/libslic3r/Arachne/WallToolPaths.cpp`
  **Route:** claude
  **Effort:** 44
  **Chars:** ~43,806 total (largest: src/libslic3r/Arachne/WallToolPaths.cpp ~43,806) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Rework pressure-advance compensation model (#12196, #11254)**
  — Two related PA-tuning requests: #12196 argues the current PA-compensation approach is
  conceptually wrong (line vs. pattern test give different optimal values); #11254 asks for
  corner-specific extrusion compensation. Directly relevant to this fork's Klipper-family
  hardware (Flashforge AD5X). PR **#10580** ("Multi Pressure Advance") in the PR queue below
  may already cover part of this — check it first.

  **Context:** `src/libslic3r/PrintConfig.cpp` · `src/libslic3r/GCode.cpp`
  **Route:** kimi
  **Effort:** 2036
  **Chars:** ~1,017,883 total (largest: src/libslic3r/PrintConfig.cpp ~566,682) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Bed temperature can no longer be set to 0°C / disabled (#12855)**
  — Regression: 2.3.2 tightened validation to reject a bed-temp value of 0 that previously
  worked to disable bed heating. A validation-tightening regression per TRIAGE_POLICY.md's
  behavior-tightening antipattern — needs evidence review, but the "0 = off" convention was
  a real, previously-supported workflow.

  **Context:** `src/libslic3r/PrintConfig.cpp`
  **Route:** kimi
  **Effort:** 567
  **Chars:** ~566,682 total (largest: src/libslic3r/PrintConfig.cpp ~566,682) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Adaptive Cubic Edge infill (#13954)**
  — New infill pattern optimized for post-fill with ballast/structural fillers (sand,
  plaster, epoxy) rather than pure mechanical strength. Touches Fill/.

  **Context:** `src/libslic3r/Fill/`
  **Route:** claude
  **Effort:** 1

- [ ] **Score 3 🥈 · Class 3 — Setting to favor shortest bridge path / manual bridge direction (#11942)**
  — Bridges over a rectangle currently span the long axis by default, causing droop;
  favoring the shortest span (or letting the user pick) would fix this. Touches bridging
  logic that reaches emitted extrusion paths.

  **Context:** `src/libslic3r/Fill/` · `src/libslic3r/LayerRegion.cpp`
  **Route:** claude
  **Effort:** 105
  **Chars:** ~52,671 total (largest: src/libslic3r/LayerRegion.cpp ~52,671) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 2 — Option to stop heating the bed N layers before print end (#12037)**
  — Lets prints start cooling sooner. Small, bounded config + G-code change.

  **Context:** `src/libslic3r/PrintConfig.cpp` · `src/libslic3r/GCode.cpp`
  **Route:** kimi
  **Effort:** 2036
  **Chars:** ~1,017,883 total (largest: src/libslic3r/PrintConfig.cpp ~566,682) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Variable layer height combined with height-range modifiers (#11549)**
  — Lets a modifier region use a different layer height than the rest of the print (e.g. a
  high-precision moving part). PR **#14012** ("Spiral Vase Mode + Height Range Modifier") in
  the PR queue below touches the same modifier machinery — check it first.

  **Context:** `src/libslic3r/Print.cpp`
  **Route:** kimi
  **Effort:** 262
  **Chars:** ~262,171 total (largest: src/libslic3r/Print.cpp ~262,171) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Tool changer: multiple nozzle sizes in a single print (#11424)**
  — Directly relevant to future toolchanging hardware (Prusa INDX) this fork plans to add.
  Distinct from the already-tracked #14145 (a nozzle-size *selector UI* for non-BBL
  printers) — this is the underlying multi-tool slicing capability itself. Touches
  multi-tool sequencing (Known Risky Subsystem).

  **Context:** `src/libslic3r/GCode/ToolOrdering.cpp` · `src/libslic3r/PrintConfig.cpp`
  **Route:** claude
  **Effort:** 1307
  **Chars:** ~653,315 total (largest: src/libslic3r/PrintConfig.cpp ~566,682) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2/3 — Spoolman integration (#2955)** — owner-approved 2026-07-05,
  medium-low priority. Connect to a Spoolman instance to look up filament IDs, track spool
  weights, and update remaining filament after a print: new `spoolman_url` setting, HTTP
  calls to the Spoolman REST API (reuse `src/slic3r/Utils/Http.cpp`), a spool picker in the
  filament panel, and a post-print hook to call Spoolman's `consume` API.
  **Architecture decision (owner, 2026-07-05):** not either/or between a GUI-layer and a
  G-code-export-layer integration — support both and let the user choose. Keep the
  G-code-side hook minimal (e.g. a small placeholder/comment marker consumed by a
  post-processing step, not embedded per-line tracking data) — G-code files are already
  constrained by unreliable-wifi upload size and limited on-printer storage, so avoid
  bloating emitted files for this. Class 3 only for the G-code-hook half of the work (touches
  emitted output); the GUI picker and setting are Class 2.
  **Candidate implementation:** open PR **#4771** ("Feature: Add Spoolman Compatability")
  implements this — large diff touching `AppConfig.cpp`, `PlaceholderParser.cpp`,
  `Preset.cpp`, `PresetBundle.cpp`, `Plater.cpp` (GUI + core + notifications). Read it against
  the dual-layer/no-bloat decision above before adopting as-is; it may need decomposition to
  separate the GUI path from the G-code-hook path.

  **Context:** `src/slic3r/Utils/Http.cpp` · `src/libslic3r/PrintConfig.cpp` · `src/libslic3r/GCode.cpp`
  **Route:** kimi
  **Effort:** 3138
  **Chars:** ~1,045,974 total (largest: src/libslic3r/PrintConfig.cpp ~566,682) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Support painting toolbox lacks a discoverable eraser tool and a "keep-out" mask to prevent accidental paint (owner pain point, 2026-07-05)**
  — Owner's complaint: the Support Painting gizmo (Paint-on supports) has no obvious toolbox
  control to erase previously-painted support marks, nor a way to paint a protected region
  that blocks accidental support painting there.
  **Partially already implemented but not discoverable:** `GLGizmoFdmSupports::on_init()`
  (`src/slic3r/GUI/Gizmos/GLGizmoFdmSupports.cpp:100-123`) already wires `Shift + Left mouse
  button` to erase and `Right mouse button` to paint a "blocker"
  (`EnforcerBlockerType::BLOCKER`, `GLGizmoFdmSupports.cpp:554`) — but both are modifier-key
  shortcuts surfaced only in a tooltip list, not selectable toolbar buttons the way the
  brush/smart-fill/gap-fill tool-type icons are (`on_render_input_window`,
  `GLGizmoFdmSupports.cpp:260`). A "blocker" paint is also not the same thing as a persistent
  "protect this region from any paint" mask the owner is asking for — today it just marks
  triangles as block-support, which a stray enforcer stroke elsewhere can still overwrite if
  painted back over the same area.
  **Fix approach:** add explicit "Eraser" and "Block" tool-type buttons alongside the existing
  Circle/Sphere/Fill/Gap-fill icons in `on_render_input_window` so both are discoverable
  without reading a shortcut tooltip, and consider making blocker triangles resist being
  silently overwritten by a later enforcer stroke (or a dedicated protect layer) so a region
  painted "no support here" survives incidental brushing.
  **Broader scope note (owner):** there's a larger open question of how far to invest in
  polished mesh-painting UX — Support painting, MMU segmentation, Fuzzy skin, and Seam
  painting all share `GLGizmoPainterBase`, so an eraser/keep-out-mask improvement made at the
  base-class level would benefit all four. If pursued more broadly, the painting toolset needs
  to be up to par with the best tools elsewhere (e.g. dedicated 3D sculpting/paint UIs), not
  just a minimal patch. Treat this entry as the narrow "eraser + keep-out mask" ask; a full
  painting-UX overhaul would need its own scoping pass.

  **Status (2026-07-09):** narrow "discoverable Eraser + Block buttons" half implemented —
  added an explicit Enforce/Block/Eraser button row to `on_render_input_window`
  (`GLGizmoFdmSupports.cpp`/`.hpp` only, not the shared `GLGizmoPainterBase` base class) that
  drives what Left-click paints via a new `get_left_button_state_type()` override
  (`m_left_click_paint_type`); Right-click still always blocks and Shift+click still always
  erases, unchanged, so existing shortcuts keep working. `handle_snapshot_action_name` updated
  to match so undo/redo history labels stay accurate. **Not yet build-verified** — this repo
  checkout has no prebuilt deps (`deps/build/` and `build/` are both empty), so this needs a
  local build + in-app click-through before merging. Not yet done: the "resist accidental
  overwrite" / persistent keep-out-mask behavior (still just a "consider" in the fix approach
  above) and the cross-gizmo base-class generalization — left for a follow-up if this is worth
  extending to MMU/Fuzzy-skin/Seam painting too.

  **Context:** `src/slic3r/GUI/Gizmos/GLGizmoFdmSupports.cpp` · `src/slic3r/GUI/Gizmos/GLGizmoFdmSupports.hpp` · `src/slic3r/GUI/Gizmos/GLGizmoPainterBase.cpp` · `src/slic3r/GUI/Gizmos/GLGizmoPainterBase.hpp`
  **Route:** claude
  **Effort:** 574
  **Chars:** ~143,557 total (largest: src/slic3r/GUI/Gizmos/GLGizmoPainterBase.cpp ~89,149) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

---

## 4. Low — cosmetic, minor, platform-specific

- [ ] **Score 1 🥉 · Class 1 — Windows on ARM build (#5783)**
  — 26 rxn. No ARM64 Windows build exists yet; CI/CD extension, not one of AGENTS.md's three
  required platforms (Windows/macOS/Linux mean x86_64 in this codebase's current build
  scripts) but low-cost to keep visible.

  **Context:** `.github/workflows/build_orca.yml`
  **Route:** gemini
  **Effort:** 31
  **Chars:** ~30,683 total (largest: .github/workflows/build_orca.yml ~30,683) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 1 — `dark_color_mode` ignored in dev Flatpak (#10034)**
  — 6 +1. Dark mode flag not propagated inside the Flatpak sandbox.

  **Context:** `src/slic3r/GUI/GLCanvas3D.cpp` · `scripts/flatpak/com.orcaslicer.OrcaSlicer.yml`
  **Route:** kimi
  **Effort:** 1006
  **Chars:** ~502,880 total (largest: src/slic3r/GUI/GLCanvas3D.cpp ~488,282) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 1 — Printables integration broken on Flatpak (#8720)**
  — 1 +1. Lowest-demand item carried over; kept visible rather than dropped per the
  "mark low, don't drop" rule, but bottom of the queue.

  **Context:** `scripts/flatpak/com.orcaslicer.OrcaSlicer.yml`
  **Route:** gemini
  **Effort:** 15
  **Chars:** ~14,598 total (largest: scripts/flatpak/com.orcaslicer.OrcaSlicer.yml ~14,598) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 3 — Slowed Overhang disables arc fitting for the entire path (#7428)**
  — owner-approved 2026-07-05, low priority. When "Slow Down for Overhang" is active on any
  path, arc fitting (G2/G3) is bypassed for the ENTIRE path, even for non-overhang segments
  of the same extrusion loop. 5 +1, 12 comments.
  **Root cause:** in `GCode::_extrude` (`src/libslic3r/GCode.cpp`), when `variable_speed` is
  true (overhang slowing active), the code iterates through `new_points` and emits one G1
  per point — the arc-fitting path is only reached when `variable_speed` is false. Since
  `variable_speed` is set per-path (not per-segment), any path with even one overhang point
  loses arc fitting for the whole path. Feeds directly into emitted G-code (G-code exporter,
  Known Risky Subsystem), so Class 3 despite the low score.
  **Fix approach:** in the variable-speed loop, detect non-variable-speed arc segments from
  `path.polyline.fitting_result` and emit G2/G3 for those while keeping G1 for
  variable-speed overhang segments — requires careful index tracking between `new_points`
  (per-pixel subdivided) and `fitting_result` (pre-overhang subdivision). Complex mixed
  G1/G2/G3 rewrite; needs design review and a slice-and-inspect verification pass before
  merge, not an unattended change.

  **Context:** `src/libslic3r/GCode.cpp`
  **Route:** claude
  **Effort:** 451
  **Chars:** ~451,201 total (largest: src/libslic3r/GCode.cpp ~451,201) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

---

## 5. Infrastructure / CI

- [ ] **Score 3 🥈 · Class 1 — Flatpak runtime on EOL `org.gnome.Platform` branch**
  — Flatpak manifest still targets an end-of-life GNOME runtime branch; update to a current
  LTS branch before it stops receiving security updates. Packaging-only, no print-correctness
  risk, but higher score than typical Class 1 polish because EOL runtimes are a real (if slow)
  security exposure.

  **Implement:** `scripts/flatpak/com.orcaslicer.OrcaSlicer.yml`
  **Route:** gemini
  **Effort:** 15
  **Chars:** ~14,598 total (largest: scripts/flatpak/com.orcaslicer.OrcaSlicer.yml ~14,598) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 1 — Build RPM workflow PR (#12983)**
  — Evaluate and merge; adds an RPM packaging CI target. No existing RPM workflow file found
  in `.github/workflows/` — this would be a new file, or the PR's own CI addition adapted in.

  **Context:** `.github/workflows/build_orca.yml`
  **Route:** gemini
  **Effort:** 31
  **Chars:** ~30,683 total (largest: .github/workflows/build_orca.yml ~30,683) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

---

## 6. PR Review Queue — open PRs not yet diff-reviewed

Per `TRIAGE_POLICY.md`: "read the diff first, not the description." These were carried over
from `TODO.md` by title/status only — none has had its actual diff read yet. Each needs the
full Decision Gate (antipatterns, bundle/monolith check, conflict-freedom) applied at
diff-read time, not assumed from the title here.

- [ ] **Score 1 🥉 · Class 2 — New Boost library (1.91.0) (#13536)** — dependency major-version
  jump. `TRIAGE_POLICY.md`'s Major Version Jump rule applies: read Boost's migration notes and
  grep call sites for removed/changed APIs before treating this as a Security-patch-tier
  drop-in; `deps/Boost` already carries local patches (`0001-Boost-fix.patch`) that may not
  apply cleanly to 1.91.0.

  **Context:** `deps/Boost/Boost.cmake`
  **Route:** gemini
  **Effort:** 2
  **Chars:** ~1,769 total (largest: deps/Boost/Boost.cmake ~1,769)

- [ ] **Score 3 🥈 · Class 2 — Nozzle size selector for non-BBL multi-extruder printers (#14145)**
  — directly relevant to future Prusa INDX (multi-tool, non-Bambu) hardware.

  **Context:** `src/slic3r/GUI/Tab.cpp`
  **Route:** kimi
  **Effort:** 428
  **Chars:** ~428,254 total (largest: src/slic3r/GUI/Tab.cpp ~428,254) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 3 — Top/bottom fill order control (Outward/Inward) (#14179)** —
  changes fill order, which ends up in emitted G-code path order.

  **Context:** `src/libslic3r/Fill/`
  **Route:** claude
  **Effort:** 1

- [ ] **Score 3 🥈 · Class 3 — Retract amount after wipe (#11015)** — touches retraction/wipe
  transitions (Known Risky Subsystem, E-value accounting).

  **Context:** `src/libslic3r/Extruder.cpp` · `src/libslic3r/GCodeWriter.cpp`
  **Route:** claude
  **Effort:** 116
  **Chars:** ~58,199 total (largest: src/libslic3r/GCodeWriter.cpp ~51,932) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 3 — Add 'brim layers' setting (#13824)** — new brim-layer capability;
  affects extruded brim G-code, so Class 3 despite being a config addition.

  **Context:** `src/libslic3r/Print.cpp`
  **Route:** claude
  **Effort:** 262
  **Chars:** ~262,171 total (largest: src/libslic3r/Print.cpp ~262,171) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 1 — Gradient filament color preview (#14324)** — visual preview
  only, no G-code path.

  **Context:** `src/slic3r/GUI/Plater.cpp`
  **Route:** kimi
  **Effort:** 834
  **Chars:** ~833,541 total (largest: src/slic3r/GUI/Plater.cpp ~833,541) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 3 — IDEX/IQEX parallel-printing support, "IMEX" (#13086)** — directly
  matches this fork's future Prusa INDX (toolchanging) hardware scope; touches
  retraction/multi-tool sequencing. ~6,000 lines — exceeds the 1,500-line decomposition
  threshold; needs a human-reviewed seam-finding pass before adoption despite high relevance.

  **Context:** `src/libslic3r/GCode.cpp` · `src/libslic3r/GCodeWriter.cpp` · `src/libslic3r/Print.cpp`
  **Route:** claude
  **Effort:** 2296
  **Chars:** ~765,304 total (largest: src/libslic3r/GCode.cpp ~451,201) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 3 — Ensure spiral lift stays inside print area, avoid nozzle collision (#13634)**
  — Prevents the nozzle striking the printed part or frame during a spiral lift move.
  Clean single-file diff, real failure-prevention value.

  **Context:** `src/libslic3r/GCodeWriter.cpp`
  **Route:** claude
  **Effort:** 52
  **Chars:** ~51,932 total (largest: src/libslic3r/GCodeWriter.cpp ~51,932) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 2 — Fix Moonraker "Happy Hare" AMS sync (#13372)** — fixes a crash
  (missing color metadata post-sync) plus a sync-path bug, directly relevant to this fork's
  owned Klipper-family hardware.

  **Context:** `src/slic3r/GUI/Plater.cpp` · `src/slic3r/Utils/MoonrakerPrinterAgent.cpp`
  **Route:** kimi
  **Effort:** 1826
  **Chars:** ~913,003 total (largest: src/slic3r/GUI/Plater.cpp ~833,541) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 3 — "Ported Wipe Tower Features Fix" (#12742)** — touches
  `WipeTower`/`WipeTower2` (multi-tool sequencing, Known Risky Subsystem), same area as the
  already-tracked #10971. The PR's own description admits it bundles four distinct fixes
  (skip points, pre-extrusion length, spiral-tower ironing, internal ribs) —
  **decompose before taking any part**, per the bundle antipattern.

  **Context:** `src/libslic3r/GCode/WipeTower.cpp` · `src/libslic3r/GCode/WipeTower2.cpp`
  **Route:** claude
  **Effort:** 651
  **Chars:** ~325,587 total (largest: src/libslic3r/GCode/WipeTower.cpp ~203,606) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 3 — Floating final purge line fix (#14416)** — real correctness fix
  (purge line ends up detached/floating) in the G-code exporter. Clean, small diff.

  **Context:** `src/libslic3r/GCode.cpp` · `src/libslic3r/Print.cpp`
  **Route:** claude
  **Effort:** 1427
  **Chars:** ~713,372 total (largest: src/libslic3r/GCode.cpp ~451,201) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 3 — Z-pinning for inter-layer mechanical interlocking (#12771)**
  — Substantial engineered feature (cites an ORNL paper and ASTM validation data), touches
  Support material and Fill (Known Risky Subsystems). ~1,850 lines — exceeds the
  decomposition soft threshold; a coherent monolith rather than a bundle, but flag for a
  full diff read plus print-test verification before adoption given the size.

  **Context:** `src/libslic3r/Support/SupportMaterial.cpp` · `src/libslic3r/Support/TreeSupport.cpp` · `src/libslic3r/GCode.cpp`
  **Route:** claude
  **Effort:** 2473
  **Chars:** ~824,253 total (largest: src/libslic3r/GCode.cpp ~451,201) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — New wall order: Odd-Even (#10622)** — real perimeter-ordering
  quality feature.

  **Context:** `src/libslic3r/PerimeterGenerator.cpp`
  **Route:** claude
  **Effort:** 149
  **Chars:** ~148,536 total (largest: src/libslic3r/PerimeterGenerator.cpp ~148,536) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Alternate internal walls direction within a layer (#5839)**
  — related to #9362 below (also a wall-alternation refinement) — read both together.

  **Context:** `src/libslic3r/PerimeterGenerator.cpp` · `src/slic3r/GUI/Gizmos/SeamPlacer.cpp`
  **Route:** claude
  **Effort:** 297
  **Chars:** ~148,536 total (largest: src/libslic3r/PerimeterGenerator.cpp ~148,536) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Precise Seam placement feature (#12974)** — substantial,
  widely-wanted seam feature. Bundles an unrelated Russian localization `.po` file update —
  strip that before adopting per the localization-touch antipattern.

  **Context:** `src/libslic3r/GCode/PreciseSeam.cpp` · `src/slic3r/GUI/Gizmos/SeamPlacer.cpp`
  **Route:** claude
  **Effort:** 1

- [ ] **Score 3 🥈 · Class 3 — Add multiple / 3D bed exclusion volumes (#13777)** — genuinely
  useful for custom FDM printers with bed clips, probes, or other obstacles.

  **Context:** `src/libslic3r/Print.cpp` · `src/libslic3r/GCode.cpp`
  **Route:** claude
  **Effort:** 1427
  **Chars:** ~713,372 total (largest: src/libslic3r/GCode.cpp ~451,201) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Clean top layer with small items above (#12187)** — addresses a
  common top-surface print-quality defect.

  **Context:** `src/libslic3r/PerimeterGenerator.cpp` · `src/libslic3r/PrintObject.cpp`
  **Route:** claude
  **Effort:** 843
  **Chars:** ~421,410 total (largest: src/libslic3r/PrintObject.cpp ~272,874) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Variable fan speed (#6738)** — widely-requested cooling feature,
  touches the cooling buffer and emitted fan values.

  **Context:** `src/libslic3r/GCode/CoolingBuffer.cpp` · `src/libslic3r/GCode.cpp`
  **Route:** claude
  **Effort:** 1017
  **Chars:** ~508,438 total (largest: src/libslic3r/GCode.cpp ~451,201) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Zero flow ironing (#10350)** — real print-quality feature;
  touches E-value accounting (Known Risky Subsystem — retraction/wipe).

  **Context:** `src/libslic3r/GCodeWriter.cpp` · `src/libslic3r/GCode.cpp`
  **Route:** claude
  **Effort:** 1006
  **Chars:** ~503,133 total (largest: src/libslic3r/GCode.cpp ~451,201) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Extra perimeters on overhangs vs. bridging (#10565)** — small,
  clean, targeted overhang-quality fix, single file. Related to the bridging area already
  tracked under #2231.

  **Context:** `src/libslic3r/PerimeterGenerator.cpp`
  **Route:** claude
  **Effort:** 149
  **Chars:** ~148,536 total (largest: src/libslic3r/PerimeterGenerator.cpp ~148,536) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Fix OrcaFilamentLibrary siblings hidden when generic installed (#13573)**
  — small, high-confidence bug fix, excellent impact/effort ratio.

  **Context:** `src/libslic3r/Preset.cpp`
  **Route:** kimi
  **Effort:** 204
  **Chars:** ~204,284 total (largest: src/libslic3r/Preset.cpp ~204,284) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Add a slicing tolerance option (#9801)** — touches core
  mesh-slicing math; real, widely-wanted precision control.

  **Context:** `src/libslic3r/TriangleMeshSlicer.cpp`
  **Route:** claude
  **Effort:** 128
  **Chars:** ~127,775 total (largest: src/libslic3r/TriangleMeshSlicer.cpp ~127,775) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — "Smooth" outer perimeters (#8443)** — well-known quality feature.
  Unexpectedly also touches `Support/TreeSupport.cpp` — confirm that's not an unrelated
  bundled change before taking it as-is.

  **Context:** `src/libslic3r/PerimeterGenerator.cpp` · `src/libslic3r/Layer.cpp`
  **Route:** claude
  **Effort:** 341
  **Chars:** ~170,386 total (largest: src/libslic3r/PerimeterGenerator.cpp ~148,536) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Gridify: anti-warping for large flat objects (#9258)** — new
  self-contained slicing-math feature addressing a real warping problem.

  **Context:** `src/libslic3r/PrintObject.cpp` · `src/libslic3r/PrintObjectSlice.cpp`
  **Route:** claude
  **Effort:** 720
  **Chars:** ~359,841 total (largest: src/libslic3r/PrintObject.cpp ~272,874) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Anisotropic surfaces + Separated Infills, remake (#11682)**
  — re-submission of an infill-strength feature; touches Fill/ core.

  **Context:** `src/libslic3r/Fill/Fill.cpp` · `src/libslic3r/Fill/FillBase.cpp`
  **Route:** claude
  **Effort:** 458
  **Chars:** ~229,034 total (largest: src/libslic3r/Fill/FillBase.cpp ~139,550) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Port PrusaSlicer's "consistent-surface cooling logic" (#12151)**
  — PrusaSlicer is AGPLv3 (license-compatible per the competitor-port check); real
  fan-speed/cooling-quality improvement.

  **Context:** `src/libslic3r/GCode/CoolingBuffer.cpp` · `src/libslic3r/Print.cpp`
  **Route:** claude
  **Effort:** 639
  **Chars:** ~319,408 total (largest: src/libslic3r/Print.cpp ~262,171) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Add bed-specific Z offset overrides (#14030)** — useful for
  custom setups with multiple beds/plates; Z-offset feeds directly into emitted G-code.

  **Context:** `src/libslic3r/GCode.cpp` · `src/libslic3r/Print.cpp`
  **Route:** claude
  **Effort:** 1427
  **Chars:** ~713,372 total (largest: src/libslic3r/GCode.cpp ~451,201) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Fill in truncated octahedron tops, optional setting (#12541)**
  — fixes a visible top-surface gap defect in 3D-honeycomb infill.

  **Context:** `src/libslic3r/Fill/Fill3DHoneycomb.cpp`
  **Route:** claude
  **Effort:** 12
  **Chars:** ~12,162 total (largest: src/libslic3r/Fill/Fill3DHoneycomb.cpp ~12,162) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Cyclic ordering toolpath strategy (#13578)** — new travel/ordering
  strategy with actual unit test coverage (`test_ordering_strategies.cpp`); touches G-code
  exporter ordering.

  **Context:** `src/libslic3r/GCode/OrderingStrategies.cpp` · `src/libslic3r/GCode.cpp`
  **Route:** claude
  **Effort:** 902
  **Chars:** ~451,201 total (largest: src/libslic3r/GCode.cpp ~451,201) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — No fuzzy skin on bridges (#13891)** — single-file, clean
  correctness fix; fuzzy skin is currently misapplied to bridge surfaces.

  **Context:** `src/libslic3r/Feature/FuzzySkin/FuzzySkin.cpp`
  **Route:** claude
  **Effort:** 36
  **Chars:** ~36,272 total (largest: src/libslic3r/Feature/FuzzySkin/FuzzySkin.cpp ~36,272) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Multi Pressure Advance, per-nozzle relation (#10580)** — relevant
  to future multi-tool (INDX) hardware and to the PA-model rework tracked above under
  #12196/#11254 — read together.

  **Context:** `src/libslic3r/GCode/AdaptivePAProcessor.cpp` · `src/libslic3r/GCode.cpp`
  **Route:** claude
  **Effort:** 942
  **Chars:** ~471,020 total (largest: src/libslic3r/GCode.cpp ~451,201) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Tangential Sacrificial Bridging for counterbore holes (#12109)**
  — real, self-contained new slicing feature for functional-print bridging quality.

  **Context:** `src/libslic3r/TangentialHoleBridging.cpp` · `src/libslic3r/PrintObject.cpp`
  **Route:** claude
  **Effort:** 546
  **Chars:** ~272,874 total (largest: src/libslic3r/PrintObject.cpp ~272,874) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Alternate extra wall as a number, not just a boolean (#9362)**
  — refinement of an existing wall-alternation option; related to #5839 above.

  **Context:** `src/libslic3r/PerimeterGenerator.cpp`
  **Route:** claude
  **Effort:** 149
  **Chars:** ~148,536 total (largest: src/libslic3r/PerimeterGenerator.cpp ~148,536) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Add Spiral Vase Mode to Height Range Modifier (#14012)** — real,
  widely-wanted feature with actual test coverage (`test_range_spiral_vase.cpp`). Touches
  the same modifier machinery as #11549 above — read together.

  **Context:** `src/libslic3r/GCode/SpiralVase.cpp` · `src/libslic3r/PrintObject.cpp`
  **Route:** claude
  **Effort:** 569
  **Chars:** ~284,416 total (largest: src/libslic3r/PrintObject.cpp ~272,874) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Port FillScatteredRectilinear from SuperSlicer (#12000)** — new
  infill pattern. SuperSlicer is AGPLv3 (same lineage as OrcaSlicer, license-compatible),
  but verify per the competitor-port antipattern before adopting.

  **Context:** `src/libslic3r/Fill/FillRectilinear.cpp`
  **Route:** claude
  **Effort:** 199
  **Chars:** ~199,223 total (largest: src/libslic3r/Fill/FillRectilinear.cpp ~199,223) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Practical Flow Ratio Calibration Test (#11065)** — real
  calibration-workflow feature. Touches unrelated `Emboss.cpp`/`Model.cpp` alongside the
  calibration code — flag for decomposition before taking the whole diff.

  **Context:** `src/slic3r/GUI/calib_dlg.cpp` · `src/libslic3r/Fill/FillBase.cpp`
  **Route:** kimi
  **Effort:** 416
  **Chars:** ~208,062 total (largest: src/libslic3r/Fill/FillBase.cpp ~139,550) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Detailed cost breakdown: electricity, wear, maintenance, tax (#13906)**
  — real, widely-useful cost-tracking feature, config/UI only. Bundles `.pot`/`es.po`
  localization file changes — strip those before adopting.

  **Context:** `src/libslic3r/PrintConfig.cpp` · `src/slic3r/GUI/Tab.cpp`
  **Route:** kimi
  **Effort:** 1990
  **Chars:** ~994,936 total (largest: src/libslic3r/PrintConfig.cpp ~566,682) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 1 — Fix dropdown popup immediately closing on some window managers (#11606)**
  — title says Qubes-only but the PR body links four separate long-standing reports on
  other window managers too. One-line fix, already published upstream in BambuStudio,
  near-zero risk.

  **Context:** `src/slic3r/GUI/Widgets/PopupWindow.cpp`
  **Route:** gemini
  **Effort:** 3
  **Chars:** ~3,341 total (largest: src/slic3r/GUI/Widgets/PopupWindow.cpp ~3,341)

- [ ] **Score 3 🥈 · Class 1 — Add Align/Distribute objects on the print plate (#13373)**
  — widely useful, self-contained new GUI gizmo; no G-code path involvement.

  **Context:** `src/slic3r/GUI/Gizmos/GLGizmoAlignment.cpp`
  **Route:** gemini
  **Effort:** 1

- [ ] **Score 3 🥈 · Class 2 — Cache system presets to eliminate startup/wizard load time (#14217)**
  — real, self-contained performance win (cold start reported ~10-30s → milliseconds);
  preset-loading logic only, no G-code path. Relevant to the already-tracked #997
  (UI slow to respond) investigation — may share a root cause worth checking together.

  **Context:** `src/libslic3r/Preset.cpp` · `src/libslic3r/PresetBundle.cpp`
  **Route:** kimi
  **Effort:** 1025
  **Chars:** ~512,426 total (largest: src/libslic3r/PresetBundle.cpp ~308,142) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Fix bottom shell thickness + spiral vase interaction (#11496)**
  — tiny (1-line) real bug fix, excellent impact/effort ratio.

  **Context:** `src/libslic3r/ConfigManipulation.cpp`
  **Route:** gemini
  **Effort:** 1

---

## Notes

- **Shipped**: Completed items live in `shipped.md`.
- **Parent repo**: https://github.com/OrcaSlicer/OrcaSlicer — `upstream` git remote is
  configured. First sync done 2026-07-07 (`git fetch upstream && git merge upstream/main`,
  144 commits pulled in, one conflict in `src/libslic3r/GCodeWriter.cpp` resolved in favor of
  upstream's generalized per-extruder `EXTRUDER_LIMIT`/vector acceleration-limit system, which
  superseded this fork's in-progress scalar travel-acceleration patch). Cross-referencing
  those 144 commits against every `#NNNN` in this file found 4 items upstream had already
  fixed — moved to `shipped.md`: #13459 (small perimeters speed for supports), #14292 (plate
  toolbar improvements), #14333 (skirt/brim refactor), #14340 (minimal chamber temperature
  field). Re-run this same cross-reference after each future sync — see
  `TRIAGE_POLICY.md`'s header note for the sync command.
- **Do NOT touch**: `deps/`, `deps_src/`, `localization/`, `resources/profiles/` (vendor/
  community-managed), `sandboxes/`.
- **Build**: CMake + Ninja/MSVC; see `build_linux.sh` / `build_release_vs2022.bat` /
  `build_release_macos.sh`.
- **Test framework**: Catch2, tests in `tests/`.
- **CI**: GitHub Actions (`.github/workflows/`).
- **`TODO.md` retired**: fully triaged into this file (2026-07-05) and deleted — this roadmap
  is now the single source of truth for backlog items. #14216 ("Fix bridging perimeters") and
  #12470 ("Improve thick bridges spacing") are referenced under #2231 in section 2 rather
  than repeated here.
- **APPROVALS.md decisions landed (2026-07-05)**: the owner reviewed the four items queued
  in `APPROVALS.md` from the second triage pass below.
  - **#7428** (arc-fitting bug) and **#2955** (Spoolman) were approved and moved into this
    file above (Low and Medium sections respectively) with the owner's scope/priority
    constraints baked in.
  - **#14133** (wipe-tower patent exposure): owner doesn't use wipe towers personally and
    has no plan to, but wants the existing implementation kept as-is regardless — no code
    change warranted on the basis of that issue. Resolved; removed from `APPROVALS.md`, not
    added here since no action follows from it.
  - **#14318** (self-hosted Orca Cloud sync): still open. Owner has no personal stake
    (doesn't use Bambu hardware) but wants the value to other users investigated before a
    scope decision — remains in `APPROVALS.md` with initial investigation notes on how
    narrow a "self-hosted" scope would need to be relative to the full `ICloudServiceAgent`
    surface.
- **Excluded outright** (scope/vendor-path, not a priority judgment — see
  `TRIAGE_POLICY.md`'s hardware-scope section): **#12376** (Anycubic Kobra X printer profile)
  — `resources/profiles/` is a vendor/community-managed do-not-touch path. **#6885** (belt
  printer support) — no belt printer owned or planned; fundamentally different kinematics
  from every printer in `TRIAGE_POLICY.md`'s Hardware Scope, closer to an unsupported-platform
  antipattern than a low-priority feature.

- **Second triage pass (2026-07-05)**: ran the broadened `oncall-triage` skill (issues + PRs +
  feature requests, not just bug-labeled issues) and folded qualifying finds into the
  sections above. Items considered and deliberately left out of this file:
  - **Legal-risk flag, not a normal backlog item**: **#14133** asked to remove purge
    towers/wipe-tower support entirely, citing a June 2026 US PTAB ruling upholding patent
    US9421713B2 (BambuLab v. Stratasys) as valid. Resolved 2026-07-05 — see the
    "APPROVALS.md decisions landed" note above; no roadmap entry follows from it.
  - **Architecture-decision items**: **#14318** (self-hosted Orca Cloud sync) still needs a
    scope decision and remains in `APPROVALS.md` (see the note above). **#2955** (Spoolman)
    was decided and is now tracked above in section 3 with **PR #4771** noted as a
    cherry-pick candidate.
  - **Vendor/hardware-scope exclusions from this pass**: printer-profile-only requests/PRs
    for hardware this fork doesn't own or plan to own (#11552 Anycubic Kobra 3 V2, #12985
    Creality SparkX i7, #11974 H2C, PRs #13291/#14089 Creality K2/K1, PR #10342 Cryogrip
    Pro/Bambu X1C) — same `resources/profiles/` vendor-path reasoning as #12376 above.
    Bambu-exclusive cloud/account/camera/network-handshake issues (#12563, #9303, #6466,
    #5878, #7790, #6585, #13025, #12896, #13650, #14028) and PRs (#13409, #13664, #7708) —
    not owned hardware. Wayland-only issues (#6433, #8145, #11698, #10059, #11434) and PR
    #11557 — explicit antipattern. Already-tracked duplicates: #12277 (dup of #10524), #8243
    (dup of #10756).
  - **Competitor-port license check needed before any future revisit**: #12093 and #12240
    both ask to port code from "preFlight" slicer; that project's license hasn't been
    checked and both issues are low-effort-specified ("copy and paste the code idk") — not
    actionable as-is regardless of license status.
  - **WIP / dependency-antipattern PRs skipped**: #13437 (title says WIP) and #14530 ("Python
    Plugins" — vendors a full pybind11/embedded-CPython dependency and is framed by its own
    author as an RFC, not a mergeable change).
