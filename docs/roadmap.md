# OrcaSlicer (fork) — Roadmap

Instantiated from [colintrachte/Project Management Tools](../../Project%20Management%20Tools)'s
`templates/ROADMAP.template.md`. Re-prioritized against this fork's own hardware (custom FDM
printers, Flashforge Adventurer Pro 5, future Prusa INDX) per `TRIAGE_POLICY.md`'s Hardware
Scope — not upstream SoftFever/OrcaSlicer's vote counts or issue labels.

`TODO.md` has been fully triaged into this file and deleted (2026-07-05) — this is now the
single source of truth for backlog items. New triage passes should add items directly here,
in this format, so `helper_scripts/PMT/route_tasks.py`/`pack_task.py` can route and pack them.
See the Notes section at the bottom for what was intentionally left out (already tracked
elsewhere, or excluded on hardware/vendor-path scope).

---

## Format convention

`helper_scripts/PMT/route_tasks.py` parses this file; `helper_scripts/PMT/context_pack.py` and
`helper_scripts/PMT/pack_task.py` consume the file lists you write into it. Keep new items
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

## Owner execution override — 2026-08-29

This order controls pickup even where the severity sections below are ordered differently:

1. Build Windows Release only when the owner needs to perform a human slicing test, and do not poll
   normal compiler progress. Automated verification and agent-only work use focused development
   targets. RelWithDebInfo is not a substitute for owner slicing because OrcaSlicer disables
   optimization and enables runtime checks in that configuration.
2. Ship **Scope nozzle/variant profile validation to FFF printers** below, including the SLA and
   malformed-FFF regression cases plus the 67-vendor validator.
3. Work the **Preset management UX** group and related compatibility defects as the primary
   product stream. Prioritize reliable preset selection/import/recovery and explanations that let
   the owner troubleshoot without editing JSON.
4. Defer architecture pilots, unrelated upstream bugs, support work, and other improvements until
   the owner confirms preset compatibility and troubleshooting are dependable.

---

## 1. Critical — crashes, data loss

- [ ] **Score 5 🥇 · Class 2 — Crash switching to Device tab for Klipper printers (#10756)**
  **ID:** task_e240ec1b08f6dea24717589a9e90576e
  **Status:** Defined
      — WebView/Klipper UI tab crash on Linux (flatpak, AppImage, NixOS). Reported on 2.3.1+,
      still open, 135 comments. Likely a WebView2/wxWebView null-guard or deferred-init issue on
      GTK. Does not touch G-code emission — UI-layer crash.

  **Context:** `src/slic3r/GUI/PrinterWebView.cpp` · `src/slic3r/GUI/PrinterWebView.hpp` · `src/slic3r/GUI/Monitor.cpp`
  **Route:** gemini
  **Context Load:** 92
  **Chars:** ~30,740 text total (largest: src/slic3r/GUI/Monitor.cpp ~18,162) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 2 — Crash on startup due to Pango/freetype2 (#10524)** — Linux
  **ID:** task_d5c18c1b4ae41550388d723ec3d17e27
  **Status:** Defined
      only, 10 +1, 49 comments. Font/rendering initialization path.

  **Context:** `src/slic3r/GUI/GUI_App.cpp` · `src/slic3r/GUI/GUI_App.hpp`
  **Route:** kimi
  **Context Load:** 894
  **Chars:** ~447,007 text total (largest: src/slic3r/GUI/GUI_App.cpp ~412,229) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 2 — Crash opening Preferences (#11641)** — 8 +1, 11 comments.
  **ID:** task_b36098c9450d4325ed7d5b12205b9869
  **Status:** Defined
      Investigate preferences dialog initialization.

  **Implement:** `src/slic3r/GUI/Preferences.cpp`
  **Route:** kimi
  **Context Load:** 101
  **Chars:** ~100,762 text total (largest: src/slic3r/GUI/Preferences.cpp ~100,762) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 2 — Segfault on startup, Debian 13 / Arch AUR builds (#10434)**
  **ID:** task_789608aa229842fd5f458d3dadd5978e
  **Status:** Defined
      — X11 (not Wayland) crash before a printer can even be selected: "Cannot register URI
      scheme wxfs more than once" followed by SIGSEGV. Distinct signature from #10524
      (Pango/freetype2) despite both being crash-on-launch — no workaround found in-thread.

  **Context:** `src/slic3r/GUI/Widgets/WebView.cpp` · `src/slic3r/GUI/WebViewDialog.cpp`
  **Route:** gemini
  **Context Load:** 101
  **Chars:** ~50,668 text total (largest: src/slic3r/GUI/WebViewDialog.cpp ~31,956) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all
  (wxfs URI scheme registration — unverified, needs a diff-time confirmation pass)

- [ ] **Score 5 🥇 · Class 2 — Slicing crashes with small-area flow compensation on non-English UI (#12476)**
  **ID:** task_d4a7f65053be3d46da2897c0cc477395
  **Status:** Defined
      — High-confidence root cause: `small_area_infill_flow_compensation_model` is stored as
      `coStrings` (numeric coefficients as text) and likely fails locale-sensitive
      `atof`/`strtod` parsing under a comma-decimal locale (repros on Russian, Spanish,
      Ukrainian; not English). Blocks slicing entirely for affected users.

  **Context:** `src/libslic3r/PrintConfig.cpp` (`small_area_infill_flow_compensation_model`, ~line 4427)
  **Route:** kimi
  **Context Load:** 646
  **Chars:** ~646,429 text total (largest: src/libslic3r/PrintConfig.cpp ~646,429) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

---

## 2. High — broken functionality, major bugs

- [ ] **Score 5 🥇 · Class 2 — Scope nozzle/variant profile validation to FFF printers**
  **ID:** task_e691f14af7b6ee47040989f4b9617ce2
  **Status:** Defined
      — The CI-style validator currently rejects the valid SLA preset `Anycubic Photon D2`
      because its validation-only printer rule assumes every instantiable
      `printer_variant` begins with a numeric nozzle diameter. SLA printers correctly have
      no `nozzle_diameter` and may use a semantic variant such as `default`. After Phase 4 C3,
      gate only this nozzle/variant consistency rule on `printer_technology == FFF`; retain
      the existing model/variant membership checks for both technologies. Add regression
      coverage proving the SLA profile loads cleanly while malformed FFF numeric, suffix,
      and multi-nozzle variants remain rejected. Acceptance: the focused preset tests pass
      and `OrcaSlicer_profile_validator -p resources/profiles -l 2` loads all 67 vendors with
      no validation error. No profile JSON or vendor-version bump should be needed.

  **Implement:** `src/libslic3r/PresetBundle.cpp` · `tests/libslic3r/test_preset_bundle_loading.cpp`
  **Context:** `resources/profiles/GenericResin.json` · `resources/profiles/GenericResin/machine/Anycubic Photon D2.json` · `.github/workflows/check_profiles.yml`
  **Route:** claude
  **Context Load:** 4055
  **Chars:** ~811,088 text total (largest: src/libslic3r/PresetBundle.cpp ~462,128) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Previously working filaments now show as "incompatible" after profile changes (#12314)**
  **ID:** task_1b2836052a117a77d91ccf364a16eab8
  **Status:** Defined
      — Compatibility filtering regression; valid filament/printer combos incorrectly
      rejected. 48 comments. Settings/profile logic, not G-code emission.

  **Context:** `src/libslic3r/PrintConfig.cpp`
  **Route:** kimi
  **Context Load:** 646
  **Chars:** ~646,429 text total (largest: src/libslic3r/PrintConfig.cpp ~646,429) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

**Preset management UX — decomposed 2026-08-25 into the 7 independent sub-items below** (owner
pain point, 2026-07-05; re-prioritized to High 2026-07-05; decomposed 2026-08-25 per this item's
own instruction not to implement as one diff). Read this background block first, then pick
sub-items in Score order — don't re-bundle them back together. Owner-authored, not a single
upstream issue: the data model (system → user preset chains via an `inherits` pointer, diff-only
storage, printer/filament/process compatibility lists) is workable, but the UI surfaces almost
none of it, and several real upstream bugs make the opacity actively harmful rather than just
annoying. This is recurring friction and occasional silent data loss hit on _every_ preset
save/load/import, on the owner's daily-driver platform (Windows) and owned hardware, versus
several items still scored 5 in Critical/§1 above that are platform-gated to Linux distros/
desktop environments this fork doesn't run (#10524 Pango/freetype2 crash, #10434 X11 segfault,
#7649 en_GB locale crash) — a one-time crash on an unused platform is lower real-world priority
than chronic friction plus confirmed silent data loss on the platform actually used.

**Confirmed root causes (read from this codebase, not speculation — shared background for all
7 sub-items below; do not duplicate this research when picking one up):**
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
    preset's `inherits` onto _another user preset_ (not a system preset) throws
    `can not find parent for config ...json!` and is rejected outright, so the "parent/child"
    model only reliably works one level below a system preset.
  - **Duplicate-name handling has no merge path**: on 3MF/project import
    (`PresetCollection::load_external_preset`, `Preset.cpp:2416`), a name collision with a
    differing config synthesizes `prefix + "(" + name + ")"` (`Preset.cpp:2605`), incrementing
    `-1`, `-2`, etc. — there is no literal `"(copy)"` string, but the effect matches the
    complaint: a second, differently-scoped copy is created silently, with no prompt to
    merge, replace, or rebind. Bundle/JSON import (`PresetBundle::import_json_presets`,
    `PresetBundle.cpp:1480-1491`) instead prompts yes/no/yes-to-all/no-to-all on collision and
    silently _drops_ the import if declined — the opposite failure mode (data loss instead of
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
filtering regression — a direct symptom of the visibility-inspector sub-item's root cause),
#14217 (`shipped.md`, preset-loading performance — may share root cause investigation with
slow tab-switching #997), #13573 (`shipped.md`, printer-specific library profiles no longer
disappear behind a generic sibling — an `is_visible` bug of the same family as #12193 below).

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

**Regret rule (per `TRIAGE_POLICY.md`'s bundle-decomposition guidance): pick up the two Score 5
sub-items first** — both are confirmed data-loss vectors, not just friction, so they outrank the
Score 3 comprehension/polish items below even though the whole group originated from one report.

- [ ] **Score 5 🥇 · Class 2 — Preset import collision has two contradictory silent failure
  **ID:** task_5a22f15e71ebce3969b89d5d1528f582
  **Status:** Defined
      modes: unwanted duplication or dropped data (#8216)** — 3MF/project import
      (`PresetCollection::load_external_preset`, `Preset.cpp:2416`) silently synthesizes a
      `name(-1)`, `name(-2)`, ... duplicate on any name collision with differing config
      (`Preset.cpp:2605`) — no prompt to replace, merge, or rebind. Bundle/JSON import
      (`PresetBundle::import_json_presets`, `PresetBundle.cpp:1480-1491`) instead prompts
      yes/no/yes-to-all/no-to-all, but silently *drops* the import entirely if declined — the
      opposite failure mode (data loss instead of unwanted duplication). Fix: replace both paths
      with one explicit replace/merge/keep-both/rename prompt, so neither path can silently
      duplicate or silently drop. Confirmed upstream: **#8216** ("Copy printer/filament/process
      profiles across printers in bulk").

  **Context:** `src/libslic3r/Preset.cpp` · `src/libslic3r/PresetBundle.cpp` · `src/slic3r/GUI/SavePresetDialog.cpp`
  **Route:** kimi
  **Context Load:** 2109
  **Chars:** ~702,851 text total (largest: src/libslic3r/PresetBundle.cpp ~462,128) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 2 — Investigate whether `sync_user_preset` cloud sync needs
  **ID:** task_97e5ec4276cce36068df1efcada36a90
  **Status:** Defined
      conflict-resolution instead of last-write-wins (#14420, #14396, #13967, #14210)** — the
      "Auto sync user presets" path (`GUI_App.cpp:7551` `start_sync_user_preset()`,
      `GUI_App.cpp:7858` `stop_sync_user_preset()`, toggled from `Preferences.cpp:1042-1044`)
      pulls cloud state over local presets with no visible conflict check. Root cause is not yet
      confirmed — these are upstream reports *plausibly* tied to this path, not a diff-verified
      bug — so this is **Investigate tier first**: reproduce or rule out the overwrite before
      writing any fix. Confirmed upstream data-loss reports: **#14420** ("All process profiles
      missing after updating to 2.4.0+", 10 comments), **#14396** ("Not all process profiles
      synced to Orcacloud"), **#13967** ("Orca reset my config and I can't get it back"),
      **#14210** ("Show the user which preset has a conflict with Orca Cloud preset" — an
      existing feature request for exactly this gap).

  **Context:** `src/slic3r/GUI/GUI_App.cpp` · `src/slic3r/GUI/GUI_App.hpp` · `src/slic3r/GUI/Preferences.cpp`
  **Route:** kimi
  **Context Load:** 1643
  **Chars:** ~547,769 text total (largest: src/slic3r/GUI/GUI_App.cpp ~412,229) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — In-UI preset dependency/inheritance viewer** — `inherits`
  **ID:** task_ec5ce18b6897d642bb790976e2e87745
  **Status:** Defined
      (`Preset.hpp:308-311`) is a bare name string resolved by
      `PresetCollection::get_preset_parent()` (`Preset.cpp:3028-3050`), and diff-only storage
      (`config.diff(parent_preset->config)`, `Preset.cpp:1748-1790`) means a child's `.json`
      holds only what differs from its parent. Nothing in the UI shows this chain today, so a
      user can't tell which preset an edit actually lands in (base, an intermediate link, or a
      fork) without opening the JSON by hand. Add a read-only parents/children/"referenced by N
      presets" view. Purely additive and read-only — lowest risk of the 7 sub-items.

  **Context:** `src/libslic3r/Preset.cpp` · `src/slic3r/GUI/PresetComboBoxes.cpp` · `src/slic3r/GUI/Tab.cpp`
  **Route:** kimi
  **Context Load:** 2311
  **Chars:** ~770,194 text total (largest: src/slic3r/GUI/Tab.cpp ~455,047) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — "Why isn't this preset showing?" visibility inspector (#12193,
  **ID:** task_554249893a09b53ac5ca7db8a68d608e
  **Status:** Defined
      #3497)** — `Preset::is_visible` (`Preset.hpp:227`) is set by
      `set_visible_from_appconfig()` (`Preset.cpp:818-847`) from AppConfig printer-variant/
      filament-install markers; combo boxes filter on it silently (`PresetComboBoxes.cpp:402-408,
      1179-1186, 1692, 2020`) with no "why isn't this here" affordance. Confirmed upstream:
      **#12193** ("Filament preset disappears after checking 'All printers' due to typo in JSON
      key ('compatible_prints' instead of 'compatible_printers')") — Orca itself wrote the wrong
      key into its own preset and silently hid it with zero error surfaced; an inspector
      surfacing the specific `is_visible`/`compatible_printers` reason would have caught this
      immediately. **#3497** ("Allow editing compatible printers... more easily", closed as
      completed) documents the export→hand-edit-JSON→reimport workaround users were reduced to
      before an in-app editor existed — re-verify that editor actually covers what this
      inspector would surface before treating it as redundant.

  **Context:** `src/libslic3r/Preset.cpp` · `src/slic3r/GUI/PresetComboBoxes.cpp`
  **Route:** kimi
  **Context Load:** 630
  **Chars:** ~315,147 text total (largest: src/libslic3r/Preset.cpp ~220,859) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Fix "detach" edge case: detaching a child preset that itself has
  **ID:** task_24802a5fb087d79bccd76595de8ac84a
  **Status:** Defined
      children needs further manual steps (#13057)** — the detach button
      (`Tab.cpp:2497-2535` `m_detach_preset_btn`, state read via `SavePresetDialog.hpp:60`
      `is_detached()`) breaks a preset's `inherits` link, but when the detached preset itself has
      children pointing at it, those children are left in an inconsistent or manually-fixable
      state. Scope the exact repro against current `Preset.cpp`/`Tab.cpp` behavior before fixing
      — the upstream issue predates several unrelated preset-system changes in this fork.

  **Context:** `src/slic3r/GUI/Tab.cpp` · `src/slic3r/GUI/SavePresetDialog.cpp` · `src/slic3r/GUI/SavePresetDialog.hpp` · `src/libslic3r/Preset.cpp`
  **Route:** claude
  **Context Load:** 2799
  **Chars:** ~699,793 text total (largest: src/slic3r/GUI/Tab.cpp ~455,047) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Document and validate the user-preset JSON schema; fix the
  **ID:** task_eae1a2f4816e4a05e033f5f058124d29
  **Status:** Defined
      misleading import error it produces today (#12223)** — **#12223** ("User process presets:
      undocumented format, silent import failures, misleading errors", closed as duplicate —
      find and check the still-open canonical issue before starting): hand-authoring or
      hand-editing a process preset from a copied system preset fails import with "You need to
      import the corresponding printer first" even when the printer is already configured, and
      the only way to discover the correct on-disk shape today is reverse-engineering a
      UI-generated file. Scope: document the schema, and replace the misleading error with one
      that names the actual validation failure.

  **Context:** `src/libslic3r/Preset.cpp` · `src/libslic3r/PresetBundle.cpp`
  **Route:** kimi
  **Context Load:** 1366
  **Chars:** ~682,987 text total (largest: src/libslic3r/PresetBundle.cpp ~462,128) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Chaining a user preset's `inherits` onto another user preset is
  **ID:** task_ae55673ff360df19bfe247ef4f5254ad
  **Status:** Defined
      rejected outright with a cryptic error (#9536)** — `PresetCollection::get_preset_parent()`
      (`Preset.cpp:3028-3050`) throws `can not find parent for config ...json!` when a user
      preset's `inherits` points at another user preset rather than a system preset, so the
      "parent/child" model only reliably works one level below a system preset. Confirmed
      upstream: **#9536** ("Support inheritance from a user profile", 19 comments). Scope
      whether to support multi-level user-preset inheritance for real, or replace the cryptic
      exception with a clear "not supported yet" message — needs a design decision before
      implementation, not just a bug fix.

  **Context:** `src/libslic3r/Preset.cpp` · `src/libslic3r/Preset.hpp`
  **Route:** kimi
  **Context Load:** 578
  **Chars:** ~288,824 text total (largest: src/libslic3r/Preset.cpp ~220,859) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Travel acceleration incorrectly limited by extrusion acceleration (#8582)**
  **ID:** task_da35e66ea10b8ade21d38d953725f0c4
  **Status:** Defined
      — Travel moves capped to extruding accel, causing slower-than-configured travels. Feeds
      directly into G-code acceleration values — Known Risky Subsystem (G-code exporter). Open
      PR **#12148** ("`travel_short_distance_acceleration`, ported from Bambu Studio") in the PR
      queue below touches the same acceleration path — verify BambuStudio source parity per
      `TRIAGE_POLICY.md` before treating it as AGPL-clean, then read the diff.

  **Context:** `src/libslic3r/GCode.cpp`
  **Route:** claude
  **Context Load:** 565
  **Chars:** ~565,156 text total (largest: src/libslic3r/GCode.cpp ~565,156) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 1 — AppImage hard dependency on `libbz2.so.1.0` (#12902)**
  **ID:** task_bf7ce5235195ca873762ac9db543877c
  **Status:** Defined
      — Ubuntu 24.04 ships `libbz2.so.1.0.8` only; symlink missing. Needs bundling or runtime
      detection. Packaging-only, no print-correctness risk.

  **Context:** `.github/workflows/build_orca.yml`
  **Route:** gemini
  **Context Load:** 40
  **Chars:** ~39,900 text total (largest: .github/workflows/build_orca.yml ~39,900) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 1 — AppImage missing `libmspack.so.0` (Ubuntu 24.04) (#12918)**
  **ID:** task_1d6608feea7dbbda1075133562db5242
  **Status:** Defined
      — Same root-cause family as #12902 above (a shared lib the AppImage assumes is present
      isn't on stock Ubuntu 24.04) but a different library — needs its own bundling fix.

  **Context:** `.github/workflows/build_orca.yml`
  **Route:** gemini
  **Context Load:** 40
  **Chars:** ~39,900 text total (largest: .github/workflows/build_orca.yml ~39,900) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — "Create printer" fails to find some vendors (#5499)**
  **ID:** task_e4ef66323f1521db0521443ec6349247
  **Status:** Defined
      — Confirmed across many vendors (Creality, Bambu, Prusa, etc.) — "model not found" blocks
      profile creation entirely for affected vendors. Labeled `stale` but the repro is broad and
      structural (profile lookup, not a one-off), not actually fixed.

  **Context:** `src/slic3r/GUI/CreatePresetsDialog.cpp`
  **Route:** kimi
  **Context Load:** 268
  **Chars:** ~267,986 text total (largest: src/slic3r/GUI/CreatePresetsDialog.cpp ~267,986) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Wall prints twice as thick with narrow line width, Classic perimeter mode (#10058)**
  **ID:** task_7b5b9959f49d78ecc588bab46b084599
  **Status:** Defined
      — Confirmed root cause: when line width is narrower than the model wall, Classic-mode
      perimeter generation lays down two overlapping layers at the same height (workaround:
      switch to Arachne). Real print-quality/material-waste bug in wall generation.

  **Context:** `src/libslic3r/PerimeterGenerator.cpp` (Classic perimeter path — exact function not yet
  **Route:** claude
  **Context Load:** 174
  **Chars:** ~173,787 text total (largest: src/libslic3r/PerimeterGenerator.cpp ~173,787) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all
  pinned down, confirm at diff-read time)

- [ ] **Score 3 🥈 · Class 3 — Start G-code uses non-blocking M104 instead of M109, print starts before hotend at temp (#4337)**
  **ID:** task_026f51610ba84925ff26ec847137c943
  **Status:** Defined
      — Confirmed on Voxelab/Anycubic and other non-Bambu printers: generated start G-code
      doesn't wait for target hotend temp before extruding. A manual M109 workaround exists in
      custom G-code, but the emitted-G-code root cause is directly fixable.

  **Context:** `src/libslic3r/GCode.cpp` (M104/M109 emission, ~lines 1232, 1324, 4047)
  **Route:** claude
  **Context Load:** 565
  **Chars:** ~565,156 text total (largest: src/libslic3r/GCode.cpp ~565,156) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Color-change sections force fan to 100%, ignoring filament profile min/max (#3677)**
  **ID:** task_75c9851e0bafd1bcf5909f7fc40992c5
  **Status:** Defined
      — Confirmed by a second reporter: filament-profile fan-speed bounds are ignored
      specifically during color-change sections, forcing 100% against e.g. a PETG profile's
      20-35% limit. Real, verifiable fan-speed (M106) emission bug.

  **Context:** `src/libslic3r/GCode.cpp` (fan-speed/M106 emission)
  **Route:** claude
  **Context Load:** 565
  **Chars:** ~565,156 text total (largest: src/libslic3r/GCode.cpp ~565,156) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Crash when system language is `en` without `en_GB` locale installed (#7649)**
  **ID:** task_a773c53efc9b132ae4d1d11802da1e83
  **Status:** Defined
      — Confirmed root cause: hardcoded fallback to `en_GB` instead of a generic `en`/`en_US`
      breaks on minimal Linux installs lacking that specific locale. A workaround exists
      (install en_GB), but this is locale-_detection_ logic, not translation content — not the
      "touches localization strings" antipattern.

  **Context:** `src/slic3r/GUI/GUI_App.cpp` (wxLocale handling, ~lines 7290-7460)
  **Route:** kimi
  **Context Load:** 412
  **Chars:** ~412,229 text total (largest: src/slic3r/GUI/GUI_App.cpp ~412,229) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 2 — Blank / non-rendering viewport cluster on Linux (Nvidia, GTK) — investigate once (#11276, #10958)**
  **ID:** task_06393ab6c48f3f2f4592d844d8c061ea
  **Status:** Defined
      — Both issues show the same GTK-CRITICAL blank-viewport signature reported independently
      across Pop!_OS, Fedora, and Ubuntu 25.10/Flatpak, and likely share a root cause with
      several Wayland-only reports skipped during this pass (#6433, #8145, #11698, #10059,
      #11434) and the Nvidia-driver-only #10090 (workaround there is a driver downgrade, outside
      Orca's control). Track as a single bounded Investigate-tier item — TRIAGE_POLICY.md's
      default scope (3 files or eliminate the main hypotheses) — rather than five duplicate
      entries; do not implement per-issue fixes until a shared cause is confirmed or ruled out.

  **Context:** GTK/Nvidia rendering init path — not yet localized to a specific file
  **Route:** gemini
  **Context Load:** 1

---

## 3. Medium — quality-of-life improvements and missing features with high demand

- [ ] **Score 3 🥈 · Class 3 — Different filament profiles for outer walls, inner walls, and infill (#7106)**
  **ID:** task_3b0feb2d473545911475669714e04fd9
  **Status:** Defined
      — Per-feature filament/extruder assignment within a single multi-material setup (33 rxn).
      Assigning which extruder prints which region changes tool-change ordering, so this touches
      multi-tool sequencing (Known Risky Subsystem). Open PR **#12724** ("Custom filament profiles
      in MMU sync") addresses this area — read that diff first per `TRIAGE_POLICY.md`'s
      Cherry-pick tier before implementing from scratch.

  **Implement:** `src/libslic3r/Print.cpp` · `src/libslic3r/GCode/ToolOrdering.cpp`
  **Context:** `src/libslic3r/PrintConfig.cpp`
  **Route:** claude
  **Context Load:** 3419
  **Chars:** ~1,139,505 text total (largest: src/libslic3r/PrintConfig.cpp ~646,429) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — UI slow to respond when changing settings tabs (#997)**
  **ID:** task_786710166464f043a925b36b4b41daed
  **Status:** Defined
      — 79 comments; long-standing complaint. Root cause unknown (profile: tab-switching triggers
      expensive re-renders or re-validation) — Investigate + Implement tier per
      `TRIAGE_POLICY.md`: bound the investigation to 3 files or eliminating the main hypotheses,
      whichever comes first, before committing to a fix.

  **Context:** `src/slic3r/GUI/Tab.cpp`
  **Route:** kimi
  **Context Load:** 455
  **Chars:** ~455,047 text total (largest: src/slic3r/GUI/Tab.cpp ~455,047) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Connect to printer over USB (#6866)**
  **ID:** task_1a60c291c4e41687094a72c2eb5e0b57
  **Status:** Defined
      — 31 rxn. Direct-serial print-host path; in scope per `TRIAGE_POLICY.md`'s hardware note
      that print-host upload behavior is always in scope, and genuinely useful for the Flashforge/
      Klipper setups this fork owns when WiFi upload isn't available. Effort is high (new
      transport/protocol layer, no existing serial code found) — Impact 2, Effort 3 nets below
      the take threshold on the formula alone, but demand and hardware relevance justify keeping
      it visible rather than dropping it; revisit when a wired workflow is actually needed.

  **Context:** `src/slic3r/Utils/OctoPrint.cpp` · `src/slic3r/Utils/Moonraker.cpp`
  **Route:** gemini
  **Context Load:** 129
  **Chars:** ~64,283 text total (largest: src/slic3r/Utils/OctoPrint.cpp ~49,873) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 3 — Resin-style tree supports optimized for FDM (#8900)**
  **ID:** task_0dd8ed8b2f920d693195869388779674
  **Status:** Defined
      — 19 rxn. Touches Support material (Known Risky Subsystem) for a niche support style;
      low priority relative to the Critical/High support-adjacent work above.

  **Context:** `src/libslic3r/Support/TreeSupport.cpp` · `src/libslic3r/Support/TreeModelVolumes.cpp`
  **Route:** claude
  **Context Load:** 501
  **Chars:** ~250,306 text total (largest: src/libslic3r/Support/TreeSupport.cpp ~197,803) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 3 — Non-planar slicing (#5053)**
  **ID:** task_ca796bf9bd32158fe30e05be563bae43
  **Status:** Defined
      — 27 rxn, research item. Would touch the G-code exporter and Arachne (Known Risky
      Subsystems) if ever attempted. No owned hardware currently exploits non-planar motion —
      re-check if/when hardware capable of it (5-axis, tilting bed, etc.) is added.

  **Context:** `src/libslic3r/GCode.cpp`
  **Route:** claude
  **Context Load:** 565
  **Chars:** ~565,156 text total (largest: src/libslic3r/GCode.cpp ~565,156) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 3 — Binary G-code (`.bgcode`) support (#3564)**
  **ID:** task_8c0eeac8da6378879d8c3243efa9bdee
  **Status:** Defined
      — 19 rxn. PrusaSlicer already supports `.bgcode`. New export/import path in the G-code
      exporter itself (Known Risky Subsystem), so Class 3 despite modest demand.

  **Context:** `src/libslic3r/GCode.cpp`
  **Route:** claude
  **Context Load:** 565
  **Chars:** ~565,156 text total (largest: src/libslic3r/GCode.cpp ~565,156) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 2 — Add "Pause at layer" option (#6334)**
  **ID:** task_81d09cac942c4f656d5fffb43cf00251
  **Status:** Defined
      — 19 rxn. Complements the existing "Pause at height" with a layer-index variant; a bounded
      extension of an already-working pattern.

  **Context:** `src/libslic3r/GCode.cpp`
  **Route:** kimi
  **Context Load:** 565
  **Chars:** ~565,156 text total (largest: src/libslic3r/GCode.cpp ~565,156) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 2 — Custom "prepare time" metadata for Klipper (#5796)**
  **ID:** task_073c5fe7a87e375e78743d25eeecd8de
  **Status:** Defined
      — 20 rxn. Per-printer preparation-time override so ETA estimates are accurate on Klipper —
      hardware-relevant (Klipper matters to the owned Flashforge Adventurer Pro 5) even though
      the feature itself is a small config + display change.

  **Context:** `src/libslic3r/PrintConfig.cpp` · `src/libslic3r/GCode.cpp`
  **Route:** kimi
  **Context Load:** 2423
  **Chars:** ~1,211,585 text total (largest: src/libslic3r/PrintConfig.cpp ~646,429) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 2 — Allow filament to override ALL settings, not only selected ones (#12401)**
  **ID:** task_a663d7e7a1c7f584593f757e08d75f55
  **Status:** Defined
      — 19 rxn. Config-scope widening; single-file change.

  **Context:** `src/libslic3r/PrintConfig.cpp`
  **Route:** kimi
  **Context Load:** 646
  **Chars:** ~646,429 text total (largest: src/libslic3r/PrintConfig.cpp ~646,429) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 2 — Multiple dovetail cuts (#6976)**
  **ID:** task_c2c02ed08e004a5b5bddbedb74f5bd9a
  **Status:** Defined
      — 45 rxn. Extend the existing cut-tool gizmo to support multiple cuts per object.

  **Context:** `src/slic3r/GUI/Gizmos/GLGizmoCut.cpp`
  **Route:** kimi
  **Context Load:** 173
  **Chars:** ~173,412 text total (largest: src/slic3r/GUI/Gizmos/GLGizmoCut.cpp ~173,412) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 1 — Improve Z-coordinate display placement in the UI (#5315)**
  **ID:** task_5c047ce8a66eb8b655e54f5d9c449d77
  **Status:** Defined
      — 22 rxn. Pure UI positioning, no slicing-logic risk.

  **Context:** `src/slic3r/GUI/IMSlider.cpp`
  **Route:** gemini
  **Context Load:** 81
  **Chars:** ~81,002 text total (largest: src/slic3r/GUI/IMSlider.cpp ~81,002) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Organic tree supports print in air, first ~6 layers missing (#4158)**
  **ID:** task_d493936b0408b058d373a81e829f1bb1
  **Status:** Defined
      — Acknowledged by a maintainer as inherited from upstream PrusaSlicer's organic
      tree-support implementation (also present in BambuStudio, still unfixed there). Real
      print-failure risk (unsupported floating trees) on any printer using organic supports.

  **Context:** `src/libslic3r/Support/TreeSupport3D.cpp` · `src/libslic3r/Support/TreeModelVolumes.cpp`
  **Route:** claude
  **Context Load:** 604
  **Chars:** ~301,761 text total (largest: src/libslic3r/Support/TreeSupport3D.cpp ~249,258) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Variable line width on overhangs (#12029)**
  **ID:** task_877b58b92ed58e92f0e9570552022099
  **Status:** Defined
      — Wider line width on overhang perimeters measurably improves printability of otherwise
      impossible overhangs. Touches wall generation (Arachne-adjacent).
      **Checked at pickup (2026-08-24) — Monitor tier per `TRIAGE_POLICY.md`, not cherry-picked:**
      the only linked implementation is OrcaSlicer/OrcaSlicer#12051 ("Port CrealityPrint Overhang
      Optimization"), which is **`isDraft: true`** and `mergeable: CONFLICTING`. The author's own
      PR description admits an open bug ("There is also currently a bug that causes it to apply
      to non-overhang layers, see screenshots") and that some logic ended up misplaced
      (`OverhangOptimization` class exists but "some of the logic is done in `Slicing.cpp`"
      instead). 972 additions across 24 files. Also needs a CrealityPrint license check before
      adoption (not yet verified, unlike the SuperSlicer ports elsewhere in this file which are
      confirmed AGPLv3-compatible) per the competitor-port antipattern. Revisit once the draft is
      lifted and the acknowledged bug is fixed - not before.

  **Context:** `src/libslic3r/Arachne/WallToolPaths.cpp`
  **Route:** claude
  **Context Load:** 44
  **Chars:** ~43,825 text total (largest: src/libslic3r/Arachne/WallToolPaths.cpp ~43,825) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Rework pressure-advance compensation model (#12196, #11254)**
  **ID:** task_e9461918b48ee1a97e172d4524d765a7
  **Status:** Defined
      — Two related PA-tuning requests: #12196 argues the current PA-compensation approach is
      conceptually wrong (line vs. pattern test give different optimal values); #11254 asks for
      corner-specific extrusion compensation. Directly relevant to this fork's Klipper-family
      hardware (Flashforge AD5X). PR **#10580** ("Multi Pressure Advance") in the PR queue below
      may already cover part of this — check it first.

  **Context:** `src/libslic3r/PrintConfig.cpp` · `src/libslic3r/GCode.cpp`
  **Route:** kimi
  **Context Load:** 2423
  **Chars:** ~1,211,585 text total (largest: src/libslic3r/PrintConfig.cpp ~646,429) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Bed temperature can no longer be set to 0°C / disabled (#12855)**
  **ID:** task_e9b350916492320d88f9e282e33badfd
  **Status:** Defined
      — Regression: 2.3.2 tightened validation to reject a bed-temp value of 0 that previously
      worked to disable bed heating. A validation-tightening regression per TRIAGE_POLICY.md's
      behavior-tightening antipattern — needs evidence review, but the "0 = off" convention was
      a real, previously-supported workflow.

  **Context:** `src/libslic3r/PrintConfig.cpp`
  **Route:** kimi
  **Context Load:** 646
  **Chars:** ~646,429 text total (largest: src/libslic3r/PrintConfig.cpp ~646,429) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Adaptive Cubic Edge infill (#13954)**
  **ID:** task_ed9f608cba06c9771696e8cf88741978
  **Status:** Defined
      — New infill pattern optimized for post-fill with ballast/structural fillers (sand,
      plaster, epoxy) rather than pure mechanical strength. Touches Fill/.

  **Context:** `src/libslic3r/Fill/FillAdaptive.cpp` · `src/libslic3r/Fill/Fill.cpp`
  **Route:** claude
  **Context Load:** 342
  **Chars:** ~170,906 text total (largest: src/libslic3r/Fill/Fill.cpp ~95,234) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Setting to favor shortest bridge path / manual bridge direction (#11942)**
  **ID:** task_d34e5ea3f5d44127514b2af135a602bd
  **Status:** Defined
      — Bridges over a rectangle currently span the long axis by default, causing droop;
      favoring the shortest span (or letting the user pick) would fix this. Touches bridging
      logic that reaches emitted extrusion paths.

  **Context:** `src/libslic3r/Fill/FillBase.cpp` · `src/libslic3r/LayerRegion.cpp`
  **Route:** claude
  **Context Load:** 386
  **Chars:** ~192,947 text total (largest: src/libslic3r/Fill/FillBase.cpp ~140,276) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 2 — Option to stop heating the bed N layers before print end (#12037)**
  **ID:** task_93491efc7b4c7fef27911aad29b16f81
  **Status:** Defined
      — Lets prints start cooling sooner. Small, bounded config + G-code change.

  **Context:** `src/libslic3r/PrintConfig.cpp` · `src/libslic3r/GCode.cpp`
  **Route:** kimi
  **Context Load:** 2423
  **Chars:** ~1,211,585 text total (largest: src/libslic3r/PrintConfig.cpp ~646,429) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Variable layer height combined with height-range modifiers (#11549)**
  **ID:** task_118b3f549ed033996a6951e3cd7ab1b8
  **Status:** Defined
      — Lets a modifier region use a different layer height than the rest of the print (e.g. a
      high-precision moving part). PR **#14012** ("Spiral Vase Mode + Height Range Modifier") in
      the PR queue below touches the same modifier machinery — check it first.

  **Context:** `src/libslic3r/Print.cpp`
  **Route:** kimi
  **Context Load:** 314
  **Chars:** ~314,005 text total (largest: src/libslic3r/Print.cpp ~314,005) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2/3 — Spoolman integration (#2955)** — owner-approved 2026-07-05,
  **ID:** task_4f897d8707f90575e9b6b11fdad4f210
  **Status:** Defined
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
  **Route:** claude
  **Context Load:** 3721
  **Chars:** ~1,240,210 text total (largest: src/libslic3r/PrintConfig.cpp ~646,429) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Support painting toolbox lacks a discoverable eraser tool and a "keep-out" mask to prevent accidental paint (owner pain point, 2026-07-05)**
  **ID:** task_fc22567f2bb3cceb32448777f5167224
  **Status:** Defined
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
  **Context Load:** 588
  **Chars:** ~147,018 text total (largest: src/slic3r/GUI/Gizmos/GLGizmoPainterBase.cpp ~89,145) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — 3D navigator cube should be repositionable (owner pain point, 2026-08-25)**
  **ID:** task_97710d1772ede57b798cd997284b849d
  **Status:** Defined
      — Owner's request: the navigator cube's default position (bottom-left) should stay as-is
      (the preview screen covers the top-right area with its own UI), but it should be
      repositionable via a right-click context menu on the cube itself:
      1. Top item: a "Move" button/icon that arms a drag-to-reposition mode (dragging the cube
         currently rotates the camera view instead — this needs to be a distinct mode, not the
         default drag behavior).
      2. A "snap to grid" numeric field in px (default `0` = continuous/no snapping; e.g. `8`
         snaps the position to every 8px).
      3. A "margin" numeric field in px (minimum allowed distance from the window edges; default
         should reflect whatever the current effective margin already is).
      Position should stay sensible across window resizes ("relative to the window it is in"),
      not just a fixed absolute pixel coordinate.
      **Investigated 2026-08-25, not yet implemented — findings for whoever picks this up:**
      - The cube is `GLCanvas3D::_render_3d_navigator()` (`GLCanvas3D.cpp:6081`), an
        `ImGuizmo::ViewManipulate` widget. Position is currently hardcoded:
        `ImVec2(viewManipulateLeft, viewManipulateTop - size)` with `viewManipulateLeft = 0` and
        `viewManipulateTop = io.DisplaySize.y` — i.e. flush against the bottom-left corner with an
        implicit margin of `0`, confirming `0` is the correct default for the new margin field.
      - `m_canvas_toolbar_pos[0]` (set to the cube's `size` right after the `ViewManipulate` call)
        is read by `_render_canvas_toolbar()` (`GLCanvas3D.cpp:9396`, used around line 9407-9411)
        to shift a second, bottom-pinned toolbar rightward so it doesn't overlap the cube. This
        reservation logic assumes the cube is at the bottom-left; once the cube can move, it
        should probably only reserve that space when the cube's anchor is still bottom-left (0 in
        every other case) - full general collision-avoidance for all four corners against that
        toolbar's actual rendered width is more than this task needs.
      - Suggested position model: track an anchor corner (nearest of the 4) plus a pixel offset
        from that corner's two adjacent edges, recomputed/clamped against the current window size
        every frame (rather than one absolute pixel coordinate) - this is what makes the position
        "relative to the window" across resizes. On drag-end, re-derive the nearest corner from
        the drop position and store the offset relative to it.
      - While in "Move" mode, `ViewManipulate`'s own drag-to-rotate handling needs to be
        suppressed the same way the function already suppresses it for other conflicting drags
        (the existing `other_drag_active` → `io.MouseDown[0] = false` pattern at the top of the
        function) - otherwise a reposition-drag would also spin the camera.
      - This file already has a right-click canvas-options popup to use as a style/pattern
        reference (`GLCanvas3D.cpp:9432`, `ImGui::OpenPopup("CanvasToolbarMenu")` +
        `ImGui::BeginPopup`/the `create_menu_item` helper right below it) - but that's a general
        canvas-settings menu, not specific to the cube; this needs its own separate popup, hit-
        tested against the cube's own screen rect.
      - No "move" icon glyph currently exists in `ImGuiWrapper`'s icon table (`ImGuiWrapper.cpp`,
        the `{ImGui::VisibleIcon, "im_visible"}`-style mapping) - adding one means a new SVG plus
        a registration entry, following that same pattern (e.g. `im_move`); a text-only "Move"
        menu item is a reasonable first pass if a custom icon isn't worth the extra scope.
      - Checked GitHub for prior art before starting: no open issue or PR found for repositioning
        this widget - this would be a from-scratch implementation, not a cherry-pick.

  **Context:** `src/slic3r/GUI/GLCanvas3D.cpp` · `src/slic3r/GUI/GLCanvas3D.hpp` · `src/slic3r/GUI/ImGuiWrapper.cpp` · `src/slic3r/GUI/ImGuiWrapper.hpp`
  **Route:** claude
  **Context Load:** 2948
  **Chars:** ~736,931 text total (largest: src/slic3r/GUI/GLCanvas3D.cpp ~502,970) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

---

## 4. Low — cosmetic, minor, platform-specific

- [ ] **Score 1 🥉 · Class 1 — Windows on ARM build (#5783)**
  **ID:** task_b0bf155174813fe8e86c754579443c55
  **Status:** Defined
      — 26 rxn. No ARM64 Windows build exists yet; CI/CD extension, not one of AGENTS.md's three
      required platforms (Windows/macOS/Linux mean x86_64 in this codebase's current build
      scripts) but low-cost to keep visible.

  **Context:** `.github/workflows/build_orca.yml`
  **Route:** gemini
  **Context Load:** 40
  **Chars:** ~39,900 text total (largest: .github/workflows/build_orca.yml ~39,900) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 1 — `dark_color_mode` ignored in dev Flatpak (#10034)**
  **ID:** task_62f97519d6eb898a98568fec7e04315c
  **Status:** Defined
      — 6 +1. Dark mode flag not propagated inside the Flatpak sandbox.

  **Context:** `src/slic3r/GUI/GLCanvas3D.cpp` · `scripts/flatpak/com.orcaslicer.OrcaSlicer.yml`
  **Route:** kimi
  **Context Load:** 1039
  **Chars:** ~519,588 text total (largest: src/slic3r/GUI/GLCanvas3D.cpp ~502,970) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 1 — Printables integration broken on Flatpak (#8720)**
  **ID:** task_f53f482abc0d1e1597c98a288803cc8c
  **Status:** Defined
      — 1 +1. Lowest-demand item carried over; kept visible rather than dropped per the
      "mark low, don't drop" rule, but bottom of the queue.

  **Context:** `scripts/flatpak/com.orcaslicer.OrcaSlicer.yml`
  **Route:** gemini
  **Context Load:** 17
  **Chars:** ~16,618 text total (largest: scripts/flatpak/com.orcaslicer.OrcaSlicer.yml ~16,618) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 3 — Slowed Overhang disables arc fitting for the entire path (#7428)**
  **ID:** task_144395da3944a2315d376ca2dd79cf95
  **Status:** Defined
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
  **Context Load:** 565
  **Chars:** ~565,156 text total (largest: src/libslic3r/GCode.cpp ~565,156) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

---

## 5. Infrastructure / CI

- [ ] **Measure developer compile/link configurations and the Ninja experiment**
  **ID:** task_ce3afab91ed9de264ff557ced100b3ea
  **Status:** Defined
      — Compare generator-specific trees and `DevFast`, `DevOptimized`, and `Release` options with
      repeated controlled workloads before changing global MSVC optimization/debug/link flags.

- [ ] **Score 1 🥉 · Class 1 — Refresh the bundled Python runtime and synchronized packaging pins**
  **ID:** task_196ede5f11ca40d3cbd210e1671730f5
  **Status:** Defined
      — Evaluate the next supported CPython maintenance or feature release as one coordinated
      dependency change. Update the exact application/dependency version pins, ABI library name,
      source checksum, Windows staging and CPython patch applicability, macOS/Linux relocation,
      Flatpak metadata, and any packaged standard-library or plugin assumptions together. Rebuild
      dependencies from clean Windows x64 and representative macOS/Linux trees; verify the embedded
      interpreter, pybind11 module/stub generation, plugins, application packaging, and startup
      before accepting the update. A globally installed Python is not a substitute for the bundled
      runtime and must not weaken reproducible-build isolation.

  **Implement:** `CMakeLists.txt` · `deps/python3/python3.cmake` · `deps/python3/stage_windows.cmake` · `deps/python3/relocate_linux.cmake` · `scripts/flatpak/com.orcaslicer.OrcaSlicer.yml` · `build_release_macos.sh`
  **Context:** `deps/python3/01-windows-nuget.patch` · `src/CMakeLists.txt` · `src/slic3r/CMakeLists.txt`
  **Route:** claude
  **Context Load:** 1446
  **Chars:** ~160,669 text total (largest: CMakeLists.txt ~58,533) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 1 — Flatpak runtime on EOL `org.gnome.Platform` branch**
  **ID:** task_7d75f42647f70d00a88ce755e43a66bd
  **Status:** Defined
      — Flatpak manifest still targets an end-of-life GNOME runtime branch; update to a current
      LTS branch before it stops receiving security updates. Packaging-only, no print-correctness
      risk, but higher score than typical Class 1 polish because EOL runtimes are a real (if slow)
      security exposure.

  **Implement:** `scripts/flatpak/com.orcaslicer.OrcaSlicer.yml`
  **Route:** gemini
  **Context Load:** 17
  **Chars:** ~16,618 text total (largest: scripts/flatpak/com.orcaslicer.OrcaSlicer.yml ~16,618) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 1 — Build RPM workflow PR (#12983)**
  **ID:** task_b3e3b724ef53c4c74aecccfa0e7261f7
  **Status:** Defined
      — Evaluate and merge; adds an RPM packaging CI target. No existing RPM workflow file found
      in `.github/workflows/` — this would be a new file, or the PR's own CI addition adapted in.

  **Context:** `.github/workflows/build_orca.yml`
  **Route:** gemini
  **Context Load:** 40
  **Chars:** ~39,900 text total (largest: .github/workflows/build_orca.yml ~39,900) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

### Architecture evolution

These tasks implement the staged direction documented in
`research/architecture-maintainability.md` and reconciled in
`research/orcaslicer-improvement-plan.md`. Current phase status and measured gates are tracked in
`docs/architecture/improvement-dashboard.md`. They are ordered prerequisites, not a license for a
monolithic rewrite. Preserve behavior first, keep every commit buildable, and stop after any pilot
whose maintenance cost exceeds its demonstrated value. Generated schema work stays in shadow mode
until equivalence and upstream-merge cost are measured. The capability pilot may proceed after the
baseline without waiting for broad schema adoption.

- [ ] **Score 5 🥇 · Class 1 — Inventory configuration consumers and establish the migration dashboard**
  **ID:** task_8cedb8fd8b236a4ea22808e9f079803d
  **Status:** Defined
      — Map representative settings from registration and typed accessors through GUI/editor,
      CLI, presets/inheritance, validation, 3MF/import-export, and slicing consumers. Establish
      baseline counters for hand registration, settings edited in more than two ownership
      locations, vendor/model branches, GUI modules with config semantics, forbidden dependency
      edges, and upstream merge conflicts. This is evidence gathering, not schema redesign.

  **Read:** `src/libslic3r/PrintConfig.cpp` · `src/libslic3r/PrintConfig.hpp` · `src/libslic3r/Preset.cpp` · `src/libslic3r/Format/bbs_3mf.cpp` · `src/slic3r/GUI/Tab.cpp` · `src/OrcaSlicer.cpp`
  **Write:** `research/configuration-contract.md`
  **Route:** claude
  **Context Load:** 16712
  **Chars:** ~2,387,468 text total (largest: src/libslic3r/PrintConfig.cpp ~646,429) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 1 — Record architecture performance, build-time, and binary-size baselines**
  **ID:** task_13f495717bb46978d40c682cad98bff7
  **Status:** Defined
      — On one documented stable configuration, record clean and representative incremental
      builds, application/core binary size, registry load, startup/profile validation, and
      canonical slice-time trends. Start as non-gating measurements; define a material-regression
      review rule only after variance is understood. The 2026-07-28 Windows Visual Studio build
      matrix is recorded; binary size, runtime metrics, cross-platform evidence, and the blocked
      Ninja experiment remain open.

  **Read:** `CMakeLists.txt` · `src/libslic3r/CMakeLists.txt` · `tests/libslic3r/CMakeLists.txt` · `build_windows.ps1`
  **Write:** `research/architecture-baselines.md` · `scripts/architecture_metrics.ps1`
  **Route:** claude
  **Context Load:** 624
  **Chars:** ~104,025 text total (largest: CMakeLists.txt ~58,533) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 1 — Record the configuration-schema architecture decision**
  **ID:** task_a5e7899c394327491ec51121b5dad40c
  **Status:** Defined
      — Specify ownership of setting identity, type, defaults, bounds, units, translation keys,
      serialization version, deprecation/replacement, generated C++ bindings, runtime metadata,
      validation-hook interface, and migrations. Compare JSON, YAML, and TOML for deterministic
      parsing, comments, source locations, dependency cost, gettext extraction, and CMake/Python
      availability. Decide compiled-versus-header bindings and explicitly reject or defer custom
      DSLs, runtime schema interpretation, and arbitrary native plugins.

  **Read:** `research/architecture-maintainability.md` · `research/configuration-contract.md` · `research/architecture-baselines.md` · `src/libslic3r/PrintConfig.cpp` · `src/libslic3r/PrintConfig.hpp` · `CMakeLists.txt`
  **Write:** `docs/architecture/configuration-schema.md`
  **Route:** claude
  **Context Load:** 6040
  **Chars:** ~862,793 text total (largest: src/libslic3r/PrintConfig.cpp ~646,429) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 1 — Build a deterministic shadow schema generator with golden tests**
  **ID:** task_4c3c2e323d01046de3dc63dafcfb56f9
  **Status:** Defined
      — Implement the ADR's parser and generator in new local files with deterministic ordering,
      source-located errors, golden-output tests, and a `--check` mode. Generate compiled
      implementation data rather than a template-heavy public header. Keep the active
      `PrintConfig` registry untouched and do not redesign any setting in this task.

  **Implement:** `src/libslic3r/CMakeLists.txt`
  **Context:** `docs/architecture/configuration-schema.md` · `research/architecture-baselines.md`
  **Write:** `src/libslic3r/config_schema/` · `scripts/generate_config_schema.py` · `tests/config_schema/` · `cmake/`
  **Route:** kimi
  **Context Load:** 252
  **Chars:** ~36,067 text total (largest: src/libslic3r/CMakeLists.txt ~20,046) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 2 — Shadow-generate one low-risk PrintConfig setting family**
  **ID:** task_4f5a3e6a1cb20aafc0e1268be9f54353
  **Status:** Defined
      — Encode one representative setting family without activating it. Differentially compare
      IDs, types, defaults, bounds, enum encodings, serialization, CLI exposure, GUI metadata,
      and named validation hooks against the normalized active registry. A mismatch blocks
      conversion; acceptable build and binary-size deltas are also required.

  **Implement:** `src/libslic3r/config_schema/` · `tests/libslic3r/test_config.cpp` · `src/libslic3r/CMakeLists.txt`
  **Context:** `src/libslic3r/PrintConfig.cpp` · `src/libslic3r/PrintConfig.hpp` · `research/architecture-baselines.md`
  **Write:** `tests/data/config_registry/`
  **Route:** claude
  **Context Load:** 5983
  **Chars:** ~854,681 text total (largest: src/libslic3r/PrintConfig.cpp ~646,429) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 1 — Add the post-upstream schema reconciliation report**
  **ID:** task_8255704cdc02ffca8e1d9d3473878b48
  **Status:** Defined
      — Compare the active registry with the shadow schema after each upstream merge and report
      missing settings, changed registrations, and stale schema entries. Document the workflow
      for re-importing upstream semantics without mixing schema refactors with behavior changes.
      If recurring reconciliation cost exceeds the pilot's value, retain the schema as an audit
      artifact rather than making it authoritative.

  **Implement:** `scripts/generate_config_schema.py`
  **Context:** `TRIAGE_POLICY.md` · `docs/architecture/configuration-schema.md` · `tests/data/config_registry/`
  **Write:** `docs/architecture/upstream-schema-reconciliation.md`
  **Route:** kimi
  **Context Load:** 199
  **Chars:** ~39,761 text total (largest: TRIAGE_POLICY.md ~39,761) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 2 — Activate generated bindings for one setting family consumer by consumer**
  **ID:** task_f59ee1c4942853a8a363da3afd81ccd8
  **Status:** Defined
      — Cut over the proven family atomically while tracking each consumer: registration, typed
      accessor, GUI/editor, CLI, preset/3MF serialization, validation, and slicing hook. Keep
      semantics unchanged, update the migration dashboard, and leave any remaining duplicate
      table explicitly recorded rather than assumed away.

  **Implement:** `src/libslic3r/PrintConfig.cpp` · `src/libslic3r/PrintConfig.hpp` · `src/libslic3r/config_schema/`
  **Context:** `research/configuration-contract.md` · `tests/libslic3r/test_config.cpp` · `tests/libslic3r/test_3mf.cpp`
  **Route:** claude
  **Context Load:** 5364
  **Chars:** ~893,939 text total (largest: src/libslic3r/PrintConfig.cpp ~646,429) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Separate safe runtime presentation metadata from slicing semantics**
  **ID:** task_c2a24f4724050446c0733d204ed4796b
  **Status:** Defined
      — After generated bindings are stable, load versioned labels, tooltips, grouping, help
      links, and safe visibility predicates as runtime metadata while keeping stable setting
      identity, defaults, serialization, migrations, and algorithmic validation typed and
      versioned. Migrate one settings page only, preserve stable gettext message IDs and
      translator context, and load metadata at startup; hot reload remains deferred.

  **Implement:** `src/slic3r/GUI/Tab.cpp` · `src/libslic3r/PrintConfig.cpp` · `src/libslic3r/config_schema/`
  **Context:** `docs/architecture/configuration-schema.md`
  **Write:** `resources/data/config_ui/` · `tests/slic3r_gui/` · `docs/architecture/config-ui-metadata.md`
  **Route:** claude
  **Context Load:** 7710
  **Chars:** ~1,101,476 text total (largest: src/libslic3r/PrintConfig.cpp ~646,429) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 1 — Define the printer-capability contract for owned hardware**
  **ID:** task_26240591b0cbf03685c9c9a543477e46
  **Status:** Defined
      — Specify typed, versioned hardware facts needed by custom FDM, Flashforge, and future
      toolchanging printers: tool count, nozzle constraints, multi-material support, remote-print
      transport, bed leveling, camera, and lidar where applicable. Keep hardware facts distinct
      from application policy and vendor packaging; record unknown-capability behavior and
      migration rules.

  **Read:** `src/libslic3r/Preset.hpp` · `src/libslic3r/Preset.cpp` · `src/libslic3r/PresetBundle.cpp` · `resources/profiles_template/Template.json` · `TRIAGE_POLICY.md`
  **Write:** `docs/architecture/printer-capabilities.md`
  **Route:** claude
  **Context Load:** 4752
  **Chars:** ~791,989 text total (largest: src/libslic3r/PresetBundle.cpp ~462,128) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 2 — Load and validate typed printer capabilities without changing behavior**
  **ID:** task_fa7966047993e8dd9c2228c85eea27ce
  **Status:** Defined
      — Add the capability representation, startup loading, schema-version checks, structured
      diagnostics, and equivalence fixtures for representative owned/in-scope printers. Keep
      current vendor/model branches active so this task is additive and independently reversible.

  **Implement:** `src/libslic3r/Preset.hpp` · `src/libslic3r/Preset.cpp` · `src/libslic3r/PresetBundle.cpp` · `src/dev-utils/OrcaSlicer_profile_validator.cpp`
  **Context:** `docs/architecture/printer-capabilities.md` · `resources/profiles_template/Template.json`
  **Write:** `tests/libslic3r/test_printer_capabilities.cpp` · `tests/data/printer_capabilities/`
  **Route:** claude
  **Context Load:** 6237
  **Chars:** ~779,685 text total (largest: src/libslic3r/PresetBundle.cpp ~462,128) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 2 — Replace one vendor/model check family with capability queries**
  **ID:** task_6738687281976517eadea6beaa279cc8
  **Status:** Defined
      — Select one bounded branch relevant to owned hardware, snapshot old visibility/behavior,
      migrate consumers to typed capability queries, and remove only the superseded name check.
      Do not turn capability data into UI policy expressions or migrate unrelated vendors.

  **Implement:** `src/libslic3r/Preset.cpp` · `src/libslic3r/PresetBundle.cpp`
  **Context:** `docs/architecture/printer-capabilities.md` · `tests/libslic3r/test_printer_capabilities.cpp` · `research/configuration-contract.md`
  **Route:** claude
  **Context Load:** 3415
  **Chars:** ~682,987 text total (largest: src/libslic3r/PresetBundle.cpp ~462,128) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 1 — Define a transport-agnostic device print-job boundary**
  **ID:** task_b89af4b7fba99a157a87e909ba1fb5a2
  **Status:** Defined
      — Document the minimum domain interface that prevents plater/slicing code from depending on
      MQTT, OctoPrint, Moonraker, or Flashforge details. Inventory current protocol leakage and
      identify one read-only pilot seam; do not redesign network sandboxing, credentials, or
      protocol implementations in this task.

  **Read:** `src/slic3r/Utils/PrintHost.hpp` · `src/slic3r/Utils/OctoPrint.hpp` · `src/slic3r/Utils/Flashforge.hpp` · `src/slic3r/GUI/Plater.cpp`
  **Write:** `docs/architecture/device-print-job.md`
  **Route:** claude
  **Context Load:** 5255
  **Chars:** ~1,051,065 text total (largest: src/slic3r/GUI/Plater.cpp ~1,036,922) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 1 — Pilot a read-only slic3r_config CMake boundary**
  **ID:** task_a149af19f6cebc21f1c5a812d518e812
  **Status:** Defined
      — Extract the smallest configuration registry/read interface into a compiled target, keep
      write/migration paths in place, and make focused config tests link without wxWidgets or the
      full GUI. Compare clean/incremental builds and binary size with the recorded baseline; stop
      rather than broaden the split if the boundary adds more cost than value.

  The prerequisite map and budgets are complete. The first retained seam moves 39 shared JSON keys
  into `PresetJsonKeys.hpp`, removes `Config.cpp`'s dependency on the full preset header, keeps the
  config leaf loop neutral (-0.03%), and passes 19 focused tests. The compiled target remains held:
  `Config.cpp` still requires print-schema and application utility symbols, so moving it wholesale
  would not create the required focused link boundary.

  **Implement:** `src/libslic3r/CMakeLists.txt` · `src/CMakeLists.txt` · `tests/libslic3r/CMakeLists.txt`
  **Context:** `docs/architecture/module-boundaries.md` · `research/architecture-baselines.md` · `src/libslic3r/PrintConfig.hpp`
  **Route:** claude
  **Context Load:** 1128
  **Chars:** ~188,032 text total (largest: src/libslic3r/PrintConfig.hpp ~116,339) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Move configuration write and preset-loading paths behind proven targets**
  **ID:** task_5f668a2f87b8fb235b8ff0c74acf97d1
  **Status:** Defined
      — Only after the read-only pilot passes its gate, add the mutation/migration interface and a
      narrow `slic3r_presets` target. Preserve initialization order, inheritance, and load/save
      semantics; add a dependency check for the explicitly forbidden GUI edges.

  **Implement:** `src/libslic3r/CMakeLists.txt` · `src/CMakeLists.txt` · `tests/libslic3r/CMakeLists.txt`
  **Context:** `src/libslic3r/Preset.cpp` · `src/libslic3r/PresetBundle.cpp` · `docs/architecture/module-boundaries.md`
  **Route:** claude
  **Context Load:** 4432
  **Chars:** ~738,659 text total (largest: src/libslic3r/PresetBundle.cpp ~462,128) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 1 — Characterize one Plater responsibility and its state boundaries**
  **ID:** task_109726649a468273e5a6a1492687b88b
  **Status:** Defined
      — Choose one cohesive, non-G-code candidate such as selection model, printer assignment, or
      project session. Document state ownership, command ordering, existing undo transaction use,
      and one of three thread-affinity models: main-thread-only, immutable/thread-safe, or
      serialized through the existing job/event queue. Produce headless characterization cases
      before moving code.

  **Read:** `src/slic3r/GUI/Plater.cpp` · `src/slic3r/GUI/Plater.hpp` · `src/slic3r/GUI/BackgroundSlicingProcess.cpp`
  **Write:** `docs/architecture/plater-service-pilot.md` · `tests/slic3r_gui/plater_service_cases.md`
  **Route:** claude
  **Context Load:** 5630
  **Chars:** ~1,126,061 text total (largest: src/slic3r/GUI/Plater.cpp ~1,036,922) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Extract the characterized Plater service behind a headless interface**
  **ID:** task_a10128c2a4f21164414da11d066048f9
  **Status:** Defined
      — Move only the responsibility selected by the characterization task, keep domain state and
      commands free of wxWidgets types where practical, reuse the existing undo boundary, and
      leave `Plater` as coordinator. Do not bundle an undo redesign, actor framework, rendering
      changes, or unrelated cleanup.

  **Implement:** `src/slic3r/GUI/Plater.cpp` · `src/slic3r/GUI/Plater.hpp` · `src/slic3r/CMakeLists.txt`
  **Context:** `docs/architecture/plater-service-pilot.md` · `src/slic3r/GUI/BackgroundSlicingProcess.cpp`
  **Write:** `tests/slic3r_gui/`
  **Route:** claude
  **Context Load:** 6952
  **Chars:** ~1,158,698 text total (largest: src/slic3r/GUI/Plater.cpp ~1,036,922) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Define and validate a local runtime-content package manifest**
  **ID:** task_ea0d52428262a3370ac80b6a640ceb1c
  **Status:** Defined
      — Specify package ID/version, schema compatibility range, dependencies, and mandatory
      content hashes. Extend the validator for unknown keys, missing references, inheritance
      cycles, invalid capabilities, and incompatible schema versions. Packages remain local,
      startup-loaded, and non-executable; remote signing and hot reload are deferred.

  **Implement:** `src/dev-utils/OrcaSlicer_profile_validator.cpp`
  **Context:** `resources/profiles_template/Template.json` · `docs/architecture/configuration-schema.md` · `docs/architecture/printer-capabilities.md`
  **Write:** `docs/architecture/runtime-content-packages.md` · `tests/libslic3r/test_profile_packages.cpp` · `tests/data/profile_packages/`
  **Route:** kimi
  **Context Load:** 201
  **Chars:** ~28,733 text total (largest: src/dev-utils/OrcaSlicer_profile_validator.cpp ~27,457) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Add staged package activation, atomic rollback, and last-known-good recovery**
  **ID:** task_68db1d735e00d2aaca60778ca3d99103
  **Status:** Defined
      — After local manifest validation is stable, stage a candidate package, validate before
      activation, atomically switch it into use, and recover the last-known-good copy after
      failure. Keep activation at application startup; hot updates require a later explicit
      design. If multi-author remote distribution is ever enabled, add a separate mandatory
      signature/trust/key-rotation decision first.

  **Implement:** `src/libslic3r/PresetBundle.cpp` · `src/libslic3r/Preset.cpp`
  **Context:** `docs/architecture/runtime-content-packages.md` · `tests/libslic3r/test_profile_packages.cpp`
  **Route:** claude
  **Context Load:** 2732
  **Chars:** ~682,987 text total (largest: src/libslic3r/PresetBundle.cpp ~462,128) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 1 — Classify feature macros and identify one product-policy pilot**
  **ID:** task_8f53cce0fdd60a2872ba62057f30f65f
  **Status:** Defined
      — Inventory platform availability, optional dependency, debug/instrumentation, algorithm
      experiment, and product-policy macros. Only product policy is a default runtime-migration
      candidate. Select one bounded family whose implementations can safely coexist and record
      current on/off behavior across platforms.

  **Read:** `CMakeLists.txt` · `src/CMakeLists.txt` · `src/slic3r/GUI/GUI_App.cpp` · `src/slic3r/GUI/GUI_App.hpp`
  **Write:** `docs/architecture/feature-policy.md`
  **Route:** claude
  **Context Load:** 2623
  **Chars:** ~524,598 text total (largest: src/slic3r/GUI/GUI_App.cpp ~412,229) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Introduce the typed runtime feature-policy composition root**
  **ID:** task_6ad4b357fbeab23789c226c135aa69b6
  **Status:** Defined
      — Compose build capabilities, platform capabilities, release channel, user experiments,
      and active-printer capabilities behind one typed query interface. Do not migrate existing
      macro families or remove legitimate compile-time guards in this task.

  **Implement:** `src/slic3r/GUI/GUI_App.cpp` · `src/slic3r/GUI/GUI_App.hpp` · `src/CMakeLists.txt`
  **Context:** `docs/architecture/feature-policy.md` · `docs/architecture/printer-capabilities.md`
  **Write:** `tests/slic3r_gui/feature_policy_tests.cpp`
  **Route:** claude
  **Context Load:** 2796
  **Chars:** ~466,065 text total (largest: src/slic3r/GUI/GUI_App.cpp ~412,229) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Migrate one product-policy switch family with on/off equivalence tests**
  **ID:** task_c32b0f766ee151df436cf7004a181151
  **Status:** Defined
      — Replace only the family selected by the macro inventory, preserve compiled availability
      checks, verify feature-on and feature-off behavior on Windows/macOS/Linux, and update the
      migration dashboard. Retain all other macros until their own bounded pilot is justified.

  **Implement:** `src/slic3r/GUI/GUI_App.cpp` · `src/slic3r/GUI/GUI_App.hpp` · `src/CMakeLists.txt`
  **Context:** `docs/architecture/feature-policy.md` · `tests/slic3r_gui/feature_policy_tests.cpp`
  **Route:** claude
  **Context Load:** 2330
  **Chars:** ~466,065 text total (largest: src/slic3r/GUI/GUI_App.cpp ~412,229) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

---

## 6. PR Review Queue — open PRs not yet diff-reviewed

Per `TRIAGE_POLICY.md`: "read the diff first, not the description." These were carried over
from `TODO.md` by title/status only — none has had its actual diff read yet. Each needs the
full Decision Gate (antipatterns, bundle/monolith check, conflict-freedom) applied at
diff-read time, not assumed from the title here.

- [ ] **Score 1 🥉 · Class 2 — New Boost library (1.91.0) (#13536)** — dependency major-version
  **ID:** task_18006dc7499854969736aa2c791565f8
  **Status:** Defined
      jump. `TRIAGE_POLICY.md`'s Major Version Jump rule applies: read Boost's migration notes and
      grep call sites for removed/changed APIs before treating this as a Security-patch-tier
      drop-in; `deps/Boost` already carries local patches (`0001-Boost-fix.patch`) that may not
      apply cleanly to 1.91.0.

  **Context:** `deps/Boost/Boost.cmake`
  **Route:** gemini
  **Context Load:** 2
  **Chars:** ~2,128 text total (largest: deps/Boost/Boost.cmake ~2,128)

- [ ] **Score 3 🥈 · Class 2 — Nozzle size selector for non-BBL multi-extruder printers (#14145)**
  **ID:** task_f2934112a96f8267ae8f9255f0f4868e
  **Status:** Defined
      — directly relevant to future Prusa INDX (multi-tool, non-Bambu) hardware.

  **Context:** `src/slic3r/GUI/Tab.cpp`
  **Route:** kimi
  **Context Load:** 455
  **Chars:** ~455,047 text total (largest: src/slic3r/GUI/Tab.cpp ~455,047) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 3 — Add 'brim layers' setting (#13824)** — new brim-layer capability;
  **ID:** task_15188c47f5ef505cf730812593779e7b
  **Status:** Defined
      affects extruded brim G-code, so Class 3 despite being a config addition.

  **Context:** `src/libslic3r/Print.cpp`
  **Route:** claude
  **Context Load:** 314
  **Chars:** ~314,005 text total (largest: src/libslic3r/Print.cpp ~314,005) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 1 — Gradient filament color preview (#14324)** — visual preview
  **ID:** task_b2c9ee432e3e454b573ab0adc2495283
  **Status:** Defined
      only, no G-code path.

  **Context:** `src/slic3r/GUI/Plater.cpp`
  **Route:** kimi
  **Context Load:** 1037
  **Chars:** ~1,036,922 text total (largest: src/slic3r/GUI/Plater.cpp ~1,036,922) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 3 — IDEX/IQEX parallel-printing support, "IMEX" (#13086)** — directly
  **ID:** task_6a45f133feba40fff64fc984fca56f1f
  **Status:** Defined
      matches this fork's future Prusa INDX (toolchanging) hardware scope; touches
      retraction/multi-tool sequencing. ~6,000 lines — exceeds the 1,500-line decomposition
      threshold; needs a human-reviewed seam-finding pass before adoption despite high relevance.

  **Context:** `src/libslic3r/GCode.cpp` · `src/libslic3r/GCodeWriter.cpp` · `src/libslic3r/Print.cpp`
  **Route:** claude
  **Context Load:** 2816
  **Chars:** ~938,671 text total (largest: src/libslic3r/GCode.cpp ~565,156) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 2 — Fix Moonraker "Happy Hare" AMS sync (#13372)** — fixes a crash
  **ID:** task_0b78b7ffa250afa44a33f4d2b7a4ddf7
  **Status:** Defined
      (missing color metadata post-sync) plus a sync-path bug, directly relevant to this fork's
      owned Klipper-family hardware.

  **Context:** `src/slic3r/GUI/Plater.cpp` · `src/slic3r/Utils/MoonrakerPrinterAgent.cpp`
  **Route:** kimi
  **Context Load:** 2234
  **Chars:** ~1,116,994 text total (largest: src/slic3r/GUI/Plater.cpp ~1,036,922) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 5 🥇 · Class 3 — Z-pinning for inter-layer mechanical interlocking (#12771)**
  **ID:** task_c08d737fd417b3762d1909aa00c6a775
  **Status:** Defined
      — Substantial engineered feature (cites an ORNL paper and ASTM validation data), touches
      Support material and Fill (Known Risky Subsystems). ~1,850 lines — exceeds the
      decomposition soft threshold; a coherent monolith rather than a bundle, but flag for a
      full diff read plus print-test verification before adoption given the size.

  **Context:** `src/libslic3r/Support/SupportMaterial.cpp` · `src/libslic3r/Support/TreeSupport.cpp` · `src/libslic3r/GCode.cpp`
  **Route:** claude
  **Context Load:** 2823
  **Chars:** ~940,863 text total (largest: src/libslic3r/GCode.cpp ~565,156) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Alternate internal walls direction within a layer (#5839)**
  **ID:** task_e77fdabac451845e4802d89d1dd9799a
  **Status:** Defined
      — related to #9362 below (also a wall-alternation refinement) — read both together.
      **Diff-read at pickup (2026-08-24), NOT cherry-picked — flagged instead:** this is the base
      PR #9362 is built on top of. It **replaces** the existing `overhang_reverse_internal_only`
      option with a new `alternate_internal_walls` option that is behaviorally different, not
      just renamed. Old behavior: `overhang_reverse_internal_only` only mattered *when overhang-
      reversal was already triggered* (`steep_overhang_contour`/`steep_overhang_hole`), and then
      restricted that reversal to internal perimeters only. New behavior: `alternate_internal_walls`
      is an independent trigger (`if (steep_overhang_contour || steep_overhang_hole ||
      alternate_internal_walls)`) that alternates internal-wall winding direction on every layer
      unconditionally, regardless of overhang detection - a different feature (stress/warping
      reduction via alternating wall direction) wearing the old option's clothes.
      `handle_legacy()` does map `overhang_reverse_internal_only=1` → `alternate_internal_walls=1`,
      so it's not a bare break, but a profile that had the old "only reverse internal walls when
      overhang-reversing" behavior would silently start alternating internal walls on *every*
      layer after migration - a real behavior change for existing users, not just a settings-panel
      rename. Needs an owner decision (keep both as separate options vs. accept the
      reinterpretation) before this is safe to apply, not an unattended judgment call for Claude
      per `TRIAGE_POLICY.md`'s Impact≥2-needs-evidence framing extended to compat risk.

  **Context:** `src/libslic3r/PerimeterGenerator.cpp` · `src/libslic3r/GCode/SeamPlacer.cpp`
  **Route:** claude
  **Context Load:** 500
  **Chars:** ~250,189 text total (largest: src/libslic3r/PerimeterGenerator.cpp ~173,787) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 1 🥉 · Class 3 — Precise Seam placement feature (#12974)** — substantial,
  **ID:** task_59d0325c16e8ac7cb4c9236d1ea8d0e4
  **Status:** Defined
      widely-wanted seam feature. Bundles an unrelated Russian localization `.po` file update —
      strip that before adopting per the localization-touch antipattern.
      **Deprioritized 2026-08-25 (owner direction): back burner relative to maintainability,
      performance, and UX work** — re-scored 3→1 despite the "widely-wanted" framing; the
      architecture/improvement-plan work (`research/orcaslicer-improvement-plan.md`) and the
      preset-management UX decomposition above take precedence. Revisit only after that higher-
      priority work lands or the owner explicitly re-raises it.
      **Diff-read at pickup (2026-08-24), NOT cherry-picked — flagged instead:** confirmed the
      roadmap's own note is right (`localization/i18n/ru/OrcaSlicer_ru.po` is in the file list,
      unrelated to the feature). But the real diff is much bigger than "strip the localization
      file" implies: 2,562 additions across 32 files, a wholly new subsystem
      (`src/libslic3r/GCode/PreciseSeam.cpp`/`.hpp`, 1,658 new lines alone) with 6 new modifier
      subtypes, new `.3mf` project-file serialization (`Format/3mf.cpp`), new persisted `Model`/
      `ModelVolume` state (`Model.cpp`/`.hpp`), and a full new GUI object-list/context-menu/gizmo
      surface (`GUI_Factories.cpp`, `GUI_ObjectList.cpp`, `3DScene.cpp`, 8 new icon SVGs). This
      exceeds `TRIAGE_POLICY.md`'s 1,500-line decomposition threshold outright and touches
      `.3mf` backward-compatibility (`AGENTS.md`: "Backward compatibility required for .3mf
      project files... Profile/format changes need version migration handling") for a genuinely
      new persisted format extension, not just a config option — needs a real compat/round-trip
      check (old files still open; new files degrade sanely in older builds) before merge, plus
      the GUI gizmo interaction can't be verified without manual testing. Out of scope for an
      unattended pass; worth a dedicated multi-session pickup given the "widely-wanted" framing,
      not a quick cherry-pick.

  **Context:** `src/libslic3r/GCode/SeamPlacer.cpp` · `src/slic3r/GUI/Gizmos/GLGizmoSeam.cpp`
  **Route:** claude
  **Context Load:** 181
  **Chars:** ~90,395 text total (largest: src/libslic3r/GCode/SeamPlacer.cpp ~76,402) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Add multiple / 3D bed exclusion volumes (#13777)** — genuinely
  **ID:** task_a88c3c0f937372489ad494946ed32229
  **Status:** Defined
      useful for custom FDM printers with bed clips, probes, or other obstacles.

  **Context:** `src/libslic3r/Print.cpp` · `src/libslic3r/GCode.cpp`
  **Route:** claude
  **Context Load:** 1758
  **Chars:** ~879,161 text total (largest: src/libslic3r/GCode.cpp ~565,156) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Clean top layer with small items above (#12187)** — addresses a
  **ID:** task_73b573b16de616d891f55fe844db214f
  **Status:** Defined
      common top-surface print-quality defect.

  **Context:** `src/libslic3r/PerimeterGenerator.cpp` · `src/libslic3r/PrintObject.cpp`
  **Route:** claude
  **Context Load:** 960
  **Chars:** ~480,155 text total (largest: src/libslic3r/PrintObject.cpp ~306,368) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — "Smooth" outer perimeters (#8443)** — well-known quality feature.
  **ID:** task_5d1f60adb0846bd6c4db703118e4a9a0
  **Status:** Defined
      **Diff-read at pickup (2026-08-24), NOT cherry-picked — flagged instead:** confirmed the
      `Support/TreeSupport.cpp` touch is **not** a bundle — it's a real, necessary integration
      point. The feature works by introducing a new `outer_perimeter_layer_divider` config option
      that Z-subdivides each layer into finer "sub-slices" (`Layer::lsub_slices`/
      `lsub_slices_merged`, new fields) just for the outer perimeter, giving it a smoother
      vertical profile than the rest of the layer. `TreeSupport::detect_overhangs` has to read
      from the new sub-sliced geometry instead of `lslices_extrudable` when the divider is active,
      or overhang detection would silently use stale/wrong geometry - a real correctness
      dependency, not an unrelated change riding along.
      That said, this is genuinely large cross-cutting surgery on the core `Layer` data model:
      877+/-169 (1,046 total) across 15 files, `mergeable: CONFLICTING` against upstream, and it
      touches **two** Known Risky Subsystems at once (Arachne/perimeter generation via 580 lines
      in `PerimeterGenerator.cpp`, and support material via `TreeSupport.cpp`). Under the 1,500-
      line hard threshold, but the blast radius (every consumer of per-layer slice geometry needs
      to be checked for whether it also needs sub-slice awareness, not just the two touched here)
      warrants a dedicated review pass with a slice-and-inspect comparison, not an unattended
      apply-and-build. Worth a focused follow-up session.

  **Context:** `src/libslic3r/PerimeterGenerator.cpp` · `src/libslic3r/Layer.cpp`
  **Route:** claude
  **Context Load:** 393
  **Chars:** ~196,387 text total (largest: src/libslic3r/PerimeterGenerator.cpp ~173,787) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Gridify: anti-warping for large flat objects (#9258)** — new
  **ID:** task_bb6ef026e57630f89608db8f9b9cdb1d
  **Status:** Defined
      self-contained slicing-math feature addressing a real warping problem.

  **Context:** `src/libslic3r/PrintObject.cpp` · `src/libslic3r/PrintObjectSlice.cpp`
  **Route:** claude
  **Context Load:** 788
  **Chars:** ~394,067 text total (largest: src/libslic3r/PrintObject.cpp ~306,368) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Port PrusaSlicer's "consistent-surface cooling logic" (#12151)**
  **ID:** task_0b332bf55697dddc69b299075304d482
  **Status:** Defined
      — PrusaSlicer is AGPLv3 (license-compatible per the competitor-port check); real
      fan-speed/cooling-quality improvement.

  **Context:** `src/libslic3r/GCode/CoolingBuffer.cpp` · `src/libslic3r/Print.cpp`
  **Route:** claude
  **Context Load:** 750
  **Chars:** ~375,040 text total (largest: src/libslic3r/Print.cpp ~314,005) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Add bed-specific Z offset overrides (#14030)** — useful for
  **ID:** task_817deb5f521c3baef74054f86c2f6f15
  **Status:** Defined
      custom setups with multiple beds/plates; Z-offset feeds directly into emitted G-code.

  **Context:** `src/libslic3r/GCode.cpp` · `src/libslic3r/Print.cpp`
  **Route:** claude
  **Context Load:** 1758
  **Chars:** ~879,161 text total (largest: src/libslic3r/GCode.cpp ~565,156) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Fill in truncated octahedron tops, optional setting (#12541)**
  **ID:** task_20f23a49bdd483c1f5c44a5ee6ab683e
  **Status:** Defined
      — fixes a visible top-surface gap defect in 3D-honeycomb infill.
      **Diff-read at pickup (2026-08-24), NOT cherry-picked — flagged instead:** the actual PR is
      much bigger than the title suggests. It substantially rewrites `Fill3DHoneycomb.cpp`'s core
      pattern generation (237+/-125 lines: replaces `colinearPoints`/`perpendPoints`/`zip`/
      `makeActualGrid` with new `addTops`/`makeEndPoints`/`patternPoints`/`makeZigZag`), and
      several of those changes affect **every** 3D-honeycomb print regardless of whether the new
      `infill_complete_top` option is enabled — notably `layerHeight` switches from a hardcoded
      `scale_(1.0)` to `scale_(params.layer_height)`, the density/`gridSize`/`zScale` branching
      logic changes (`density > 0.42` → `density > 0.42 || layersPerModule < 2`, with the old
      else-branch re-adjustment loop removed), a new Z `startOffset` is introduced, and the grid
      alignment changes from `gridSize*4` to `gridSize*2`. That's a direct conflict with
      `AGENTS.md`'s "features gated by options must not affect existing behavior when disabled"
      rule. The PR author's own description says they haven't print-tested it ("I do not
      currently have access to a 3D printer... purely from inspecting the GCode output"). Per
      `TRIAGE_POLICY.md`'s Known-Risky-Subsystem verification note, this needs a slice-and-inspect
      comparison against a canonical 3D-honeycomb model (default settings, option **disabled**)
      before/after this diff to confirm no silent behavior change, not a blind apply-and-build —
      out of scope for an unattended pass. Re-enter as **Adapt** tier once that comparison is
      done, or decompose into "top-fill option" (additive) vs. "existing pattern math rewrite"
      (needs its own justification) per the bundle-antipattern guidance.

  **Context:** `src/libslic3r/Fill/Fill3DHoneycomb.cpp`
  **Route:** claude
  **Context Load:** 12
  **Chars:** ~12,437 text total (largest: src/libslic3r/Fill/Fill3DHoneycomb.cpp ~12,437) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Multi Pressure Advance, per-nozzle relation (#10580)** — relevant
  **ID:** task_0cb87e4ce075d9566b25c57e97a542ce
  **Status:** Defined
      to future multi-tool (INDX) hardware and to the PA-model rework tracked above under
      #12196/#11254 — read together.

  **Context:** `src/libslic3r/GCode/AdaptivePAProcessor.cpp` · `src/libslic3r/GCode.cpp`
  **Route:** claude
  **Context Load:** 1170
  **Chars:** ~584,975 text total (largest: src/libslic3r/GCode.cpp ~565,156) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Tangential Sacrificial Bridging for counterbore holes (#12109)**
  **ID:** task_0bcf7905f600f3d95e1404052679223b
  **Status:** Defined
      — real, self-contained new slicing feature for functional-print bridging quality.

  **Implement:** `src/libslic3r/PrintObject.cpp`
  **Write:** `src/libslic3r/TangentialHoleBridging.cpp`
  **Route:** claude
  **Context Load:** 613
  **Chars:** ~306,368 text total (largest: src/libslic3r/PrintObject.cpp ~306,368) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Alternate extra wall as a number, not just a boolean (#9362)**
  **ID:** task_2da496d8dd5c95204a3333677b14154f
  **Status:** Defined
      — refinement of an existing wall-alternation option; related to #5839 above.
      **Diff-read at pickup (2026-08-24), NOT cherry-picked — flagged instead:** the PR (130+/-52
      across 10 files) is built directly on top of #5839 (confirmed: diffing the two PRs shows
      identical content, only line-number/blob-hash drift) and inherits #5839's replacement of
      `overhang_reverse_internal_only` with `alternate_internal_walls`. **Correction to an earlier
      note on this item:** it's not a bare deletion with no migration - `handle_legacy()` does map
      `overhang_reverse_internal_only=1` → `alternate_internal_walls=1`. But the two options are
      not behaviorally equivalent (see #5839's own flag above for the full explanation), so the
      migration silently changes what a migrated profile actually does. Still a bundle per
      `TRIAGE_POLICY.md` (widening `alternate_extra_wall` to an int, the PR's actual title, is
      entangled with inheriting all of #5839's unrelated option-reinterpretation) and still needs
      #5839 resolved/reviewed first, not assumed compatible. `mergeable: UNKNOWN` on GitHub (stale
      PR, needs its own refresh/rebase check too).

  **Context:** `src/libslic3r/PerimeterGenerator.cpp`
  **Route:** claude
  **Context Load:** 174
  **Chars:** ~173,787 text total (largest: src/libslic3r/PerimeterGenerator.cpp ~173,787) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 3 — Add Spiral Vase Mode to Height Range Modifier (#14012)** — real,
  **ID:** task_f08af75467739b7c9612e5516261be6f
  **Status:** Defined
      widely-wanted feature with actual test coverage (`test_range_spiral_vase.cpp`). Touches
      the same modifier machinery as #11549 above — read together.

  **Context:** `src/libslic3r/GCode/SpiralVase.cpp` · `src/libslic3r/PrintObject.cpp`
  **Route:** claude
  **Context Load:** 636
  **Chars:** ~317,910 text total (largest: src/libslic3r/PrintObject.cpp ~306,368) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Practical Flow Ratio Calibration Test (#11065)** — real
  **ID:** task_6eaab4ff3afee40419b8762339afacd3
  **Status:** Defined
      calibration-workflow feature. Touches unrelated `Emboss.cpp`/`Model.cpp` alongside the
      calibration code — flag for decomposition before taking the whole diff.

  **Context:** `src/slic3r/GUI/calib_dlg.cpp` · `src/libslic3r/Fill/FillBase.cpp`
  **Route:** kimi
  **Context Load:** 442
  **Chars:** ~220,928 text total (largest: src/libslic3r/Fill/FillBase.cpp ~140,276) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Detailed cost breakdown: electricity, wear, maintenance, tax (#13906)**
  **ID:** task_ecf7dafae2bf8961a1a78c446014ef70
  **Status:** Defined
      — real, widely-useful cost-tracking feature, config/UI only. Bundles `.pot`/`es.po`
      localization file changes — strip those before adopting.

  **Context:** `src/libslic3r/PrintConfig.cpp` · `src/slic3r/GUI/Tab.cpp`
  **Route:** kimi
  **Context Load:** 2203
  **Chars:** ~1,101,476 text total (largest: src/libslic3r/PrintConfig.cpp ~646,429) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 1 — Fix dropdown popup immediately closing on some window managers (#11606)**
  **ID:** task_99e412ffc3c530311da5afc666fa61b1
  **Status:** Defined
      — title says Qubes-only but the PR body links four separate long-standing reports on
      other window managers too. One-line fix, already published upstream in BambuStudio,
      near-zero risk.

  **Context:** `src/slic3r/GUI/Widgets/PopupWindow.cpp`
  **Route:** gemini
  **Context Load:** 3
  **Chars:** ~3,341 text total (largest: src/slic3r/GUI/Widgets/PopupWindow.cpp ~3,341)

- [ ] **Score 3 🥈 · Class 1 — Add Align/Distribute objects on the print plate (#13373)**
  **ID:** task_39a852b3dc3774a38b7087d0ce6bb0af
  **Status:** Defined
      — widely useful, self-contained new GUI gizmo; no G-code path involvement.

  **Context:** `src/slic3r/GUI/Plater.cpp` · `src/slic3r/GUI/Selection.cpp`
  **Route:** kimi
  **Context Load:** 2344
  **Chars:** ~1,171,873 text total (largest: src/slic3r/GUI/Plater.cpp ~1,036,922) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

- [ ] **Score 3 🥈 · Class 2 — Fix bottom shell thickness + spiral vase interaction (#11496)**
  **ID:** task_6ab1f0f4959e1b927a172be2e3e6f3e2
  **Status:** Defined
      — tiny (1-line) real bug fix, excellent impact/effort ratio.

  **Context:** `src/slic3r/GUI/ConfigManipulation.cpp`
  **Route:** gemini
  **Context Load:** 76
  **Chars:** ~76,145 text total (largest: src/slic3r/GUI/ConfigManipulation.cpp ~76,145) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all

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
  - **WIP / dependency-antipattern PRs skipped**: #13437 remains skipped because its title says
    WIP. #14530 ("Python Plugins") is no longer a candidate: upstream merged it and the current
    branch now contains `2717c37f63`. The earlier dependency-cost objection remains useful
    architecture context, but any follow-up should be a concrete regression or maintenance item
    rather than a request to merge that already-shipped PR.

## 5. Code-review findings — 2026-08-23, uncommitted Phase 6 working tree

Found during a review of the fork's uncommitted working tree while on the build-iteration
plan's Phase 6 pilot (`ai_inbox/orcaslicer-build-iteration-plan.md`, `docs/architecture/module-boundaries.md`).
Not yet committed — run `git status`/`git diff HEAD` in `D:\Git\OrcaSlicer` before starting any
item below to confirm current state, and see `AGENTS.md` for the repo's build/test commands and
correctness constraints (backward-compat for .3mf/profiles, cross-platform, no hardcoded paths).
Four related findings from the same review pass (`wall_loops` invalidation scope, preset
version-metadata validation, and two of the six original findings below) were verified fixed in
this working tree on 2026-08-24 and moved to `shipped.md`. The two items below were **not**
verified as fixed — re-check current code before starting either.

- [ ] **Score 1 🥉 · Class 1 — New preset/config files work against Phase 6's own compilation-firewall goal**
  **ID:** task_0e1e7041abbcdd01be6e632dd156a2be
  **Status:** Defined
      — `src/libslic3r/ConfigRegistry.cpp` is compiled unconditionally into `libslic3r`, includes
      `PrintConfig.hpp` (the ~112-consumer fan-out hotspot Phase 6 is trying to shrink), and has
      zero production callers (only `tests/libslic3r/test_config_registry.cpp`).
      `PresetImportPreflight.hpp`/`PresetInspector.hpp` similarly pull in the full `Preset.hpp`
      ahead of the plan's deferred "Pilot 2: presets" boundary. None of the three is mentioned in
      `docs/architecture/module-boundaries.md`. Decide: fold into a future pilot boundary, move
      behind test-only sources, or drop if unneeded. Verified 2026-08-24: `ConfigRegistry.cpp`/
      `.hpp` still exist and still match this description — genuinely unaddressed.

  **Context:** `src/libslic3r/ConfigRegistry.cpp` · `src/libslic3r/ConfigRegistry.hpp` · `src/libslic3r/PresetImportPreflight.hpp` · `src/libslic3r/PresetInspector.hpp` · `docs/architecture/module-boundaries.md` · `ai_inbox/orcaslicer-build-iteration-plan.md`
  **Route:** kimi
  **Context Load:** 299
  **Chars:** ~49,764 text total (largest: ai_inbox/orcaslicer-build-iteration-plan.md ~23,234) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all
  **Route/Effort/Chars:** run `helper_scripts/PMT/route_tasks.py` to stamp

- [ ] **Score 1 🥉 · Class 1 — Duplicated unique-suffix naming logic across preset backup/rename paths**
  **ID:** task_e5cc09c618765c036d676dd235f57995
  **Status:** Defined
      — The "find first free suffixed path/name" idiom is hand-rolled four separate times with
      no shared helper: `PresetBundle.cpp` (import backup), and three sites in
      `PresetInspector.cpp` (project-import backup, rename backup, `unique_recovery_name`). Fix:
      extract one shared utility (e.g. on `Preset`/`PresetCollection`) and have all four call
      sites use it, per `AGENTS.md`'s "avoid duplication" rule. Verified 2026-08-24: still four
      separate hand-rolled call sites, no shared helper — genuinely unaddressed.

  **Implement:** `src/libslic3r/PresetBundle.cpp` · `src/libslic3r/PresetInspector.cpp`
  **Route:** kimi
  **Context Load:** 1046
  **Chars:** ~523,038 text total (largest: src/libslic3r/PresetBundle.cpp ~462,128) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all
  **Route/Effort/Chars:** run `helper_scripts/PMT/route_tasks.py` to stamp

## Code-review findings — 2026-08-24, discovered while verifying the #13891 fuzzy-skin cherry-pick

Found running `fff_print_tests` (`build-vs2026`, RelWithDebInfo) to verify the #13891 cherry-pick.
Neither finding is caused by or related to that change (confirmed for the first by reproducing on
a pre-change baseline; confirmed for the second via `git blame` showing the stale constant
predates the upstream merge that shifted the real value) — recorded here as separate, genuinely
unaddressed issues per `TRIAGE_POLICY.md`'s "read the diff first" discipline.

- [ ] **Score 3 🥈 · Class 1 — `[Invalidation]` tests crash the whole test binary, order-dependently**
  **ID:** task_e61f5c507156c552c5dd6f6254de9fd8
  **Status:** Defined
      — `TEST_CASE`s tagged `[Invalidation]` in `test_print.cpp` ("Incremental slicing matches a
      clean slice for representative invalidation families", "G-code-only changes preserve object
      stages and match a clean slice") intermittently SIGSEGV depending on `--order rand`'s chosen
      execution order — confirmed by running the full suite twice with different random seeds and
      getting a crash at a different point in the test list each time (once inside the first test
      family right at "layer_height", once inside the second test entirely). Excluding the tag
      (`"~[Invalidation]"`) lets the rest of the suite run clean to completion. A crash that
      depends on what ran *before* it (not the test's own inputs) points at shared/global state
      corrupted or left inconsistent by an earlier test — worth an ASan/valgrind pass or bisecting
      which preceding test corrupts state, rather than debugging the crash site itself in
      isolation. Not touching G-code output directly (it's a crash, not a wrong-output bug), but
      Class 1 undersells the actual risk if the same shared-state bug can manifest outside tests —
      re-score once the trigger is identified.

  **Context:** `tests/fff_print/test_print.cpp`
  **Route:** gemini
  **Context Load:** 29
  **Chars:** ~28,731 text total (largest: tests/fff_print/test_print.cpp ~28,731) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all
  **Route/Effort/Chars:** run `helper_scripts/PMT/route_tasks.py` to stamp

- [ ] **Score 1 🥉 · Class 1 — Stale hardcoded tree-support diagnostic constant after the 2026-08-24 upstream merge**
  **ID:** task_67bf5f3e4831776489cebaf10d9577a2
  **Status:** Defined
      — `test_support_material.cpp`'s "Organic support for a generated cantilever is valid and
      deterministic" asserts `first_diagnostics.discarded_branch_meshes == 247`; the current build
      produces 255. `git blame` shows the `247` constant was authored in this fork's own commit
      `7870e9fd9b` (2026-08-24 09:20), *before* the same day's later `Merge remote-tracking branch
      'upstream/main'` (`515421d255`) pulled in upstream's own `04e13200aa` ("Support bugfixes",
      #14678 — see the #13983 shipped entry) touching `TreeSupport.cpp`/`TreeModelVolumes.cpp`.
      The merge shifted the real branch-mesh-discard count; the test's hardcoded expectation is
      simply stale, not a sign of a regression — update the constant to 255 (after confirming 255
      is stable/deterministic across repeated runs, which the test's own second `Print repeated`
      pass already exercises) rather than treating it as a bug to fix in `TreeSupport.cpp` itself.
      Exactly the kind of drift `docs/roadmap.md`'s own post-sync cross-reference note anticipates
      — a good candidate to fold into that routine pass instead of one-off fixing here.

  **Implement:** `tests/fff_print/test_support_material.cpp`
  **Route:** gemini
  **Context Load:** 30
  **Chars:** ~30,217 text total (largest: tests/fff_print/test_support_material.cpp ~30,217) — exceeds chatgpt's ~5,000-char inline-paste budget; that file can't go to chatgpt at all
  **Route/Effort/Chars:** run `helper_scripts/PMT/route_tasks.py` to stamp

