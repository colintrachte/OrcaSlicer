# Fork Triage & Implementation Policy

This document governs how issues and PRs from upstream [SoftFever/OrcaSlicer](https://github.com/SoftFever/OrcaSlicer)
are evaluated and implemented in this fork. It is written for an AI agent executing work on behalf of a user
who wants maximum feature quality and reliability at minimum token cost.

This fork (`colintrachte/OrcaSlicer`) has an `upstream` remote configured (`https://github.com/OrcaSlicer/OrcaSlicer.git`,
the project's current org after moving off `SoftFever/OrcaSlicer`), alongside `origin`. First sync done 2026-07-07;
re-run `git fetch upstream && git log HEAD..upstream/main --oneline` periodically per §1 below, and cross-reference
any new commits' PR numbers against `docs/roadmap.md`'s open `#NNNN` items before mining issues/PRs by hand.

---

## Principles

1. **Conflict-free upstream improvements are freebies — take them by default** unless an antipattern applies.
2. **Implementation cost scales with complexity, not issue priority.** A bug with a clear fix costs less to implement
   than an investigation with no obvious cause, regardless of severity.
3. **Read only what you need.** Don't fetch an issue's full comment thread to learn it's out of scope; the title and
   first comment are usually enough.
4. **An existing PR is useful signal, and here it's a stronger one than in a typical stale fork.** Unlike
   `prusa3d/PrusaSlicer` (which largely stopped merging community engine contributions after ~2022), SoftFever/OrcaSlicer
   is a high-velocity project that merges community engine work readily. That flips the calculus: most good engine
   improvements eventually land in upstream `main` and can be picked up with a routine sync, not by mining a graveyard
   of rejected PRs. The remaining gap is (a) PRs still open because upstream's volume means good fixes can sit for
   weeks before a maintainer gets to them, and (b) work upstream will never carry because it's specific to this fork's
   hardware (see below).
5. **SLA code is out of scope** for this fork's active development. OrcaSlicer's SLA support (`SLAPrint.*`,
   `GLGizmoSlaSupports.*`) is vestigial code inherited through the PrusaSlicer→BambuStudio→OrcaSlicer lineage; there is
   no active resin-printer development on this GUI and no reason to invest in it here. Skip SLA-only issues and PRs.
6. **Accessibility is out of scope** for this fork. It requires specialist knowledge and dedicated QA we cannot
   provide. Skip accessibility-only issues.
7. **Grab nearby wins opportunistically.** If a fix reveals an obviously broken adjacent thing, fix it.
   If an opportunity would require going significantly out of scope, note it in TODO.md and move on.
   The test: "does this derail the main task?" If yes, note and move on. If no, take it and log it as
   an adjacent fix in the CHANGELOG entry so the change isn't invisible.

---

## Hardware Scope

This fork is used on:
- **Custom FDM printers** (primary) — any issues touching custom printer profiles, third-party print-host support,
  or generic FDM slicing quality are high priority
- **Flashforge Adventurer Pro 5** — uses WiFi upload via `slic3r/Utils/Flashforge.cpp`; Klipper-style G-code matters
- **Future: Prusa INDX** (toolchanging / multi-material) — multi-tool G-code, wipe tower, color changes, tool-change
  sequences will become relevant

None of this fork's owned hardware is a Bambu Lab printer. Treat Bambu-exclusive surfaces (AMS firmware handshakes,
Bambu Cloud/Bambu Handy account features, X1/P1/A1-only calibration) the way a Prusa-hardware-only fork would treat
Prusa Connect — real code in the tree, but out of scope unless it also touches something generic.

Code paths that matter: FDM slicing pipeline, G-code output, retraction/wipe, supports, print-host upload
(OctoPrint/Moonraker/Flashforge), thumbnails, settings/profiles, UI (search, preview, seams), dependency security.

Code paths that are low priority: SLA slicing (vestigial), Bambu Cloud/Bambu Handy account-only features, AMS-exclusive
firmware handshakes, X1/P1/A1-only calibration UI, macOS-specific UI quirks, Wayland support, localization.

### Quick include/exclude by file path

**IN — take unless antipattern fires:**
Any diff that touches paths containing: `libslic3r/GCode`, `libslic3r/Print`, `libslic3r/Extruder`,
`libslic3r/Support` (covers `SupportMaterial*`, `TreeSupport*`, `TreeModelVolumes*`), `libslic3r/Fill`,
`libslic3r/Arachne`, `slic3r/Utils/OctoPrint`, `slic3r/Utils/PrintHost`, `slic3r/Utils/Moonraker`,
`slic3r/Utils/Flashforge` (owned hardware), `libslic3r/GCode/Thumbnail`, `GUI/Plater`, `GUI/PhysicalPrinterDialog`,
`deps/OpenSSL`, `deps/CURL`, `deps/Boost`, `deps/TBB`

**OUT — skip unless there is also an IN-scope file:**
Diff touches *only* paths containing: `localization/i18n`, `libslic3r/SLAPrint`, `libslic3r/SLAPrintSteps`,
`GUI/Gizmos/GLGizmoSlaSupports`, `Wayland`, `sandboxes/`, `slic3r/Utils/BBLCloudServiceAgent` (Bambu-account-only
paths), `bambu_networking` (Bambu-exclusive transport)

**Ambiguous — apply the tie-breaker (any YES → IN scope; all NO → OUT scope):**
- (a) Does it change any part of the input-to-G-code pipeline (config parsing, slicing math, path planning)?
- (b) Does it change print-start, upload, or print-host handshake behavior?
- (c) Does it change a slicer calculation that ends up in emitted G-code values?

### Hardware scope edge cases

- **Multi-material without INDX** (AMS, AMS Lite, Palette, Mosaic, generic MMU): In scope for wipe tower and
  toolchange G-code logic (`WipeTower.cpp`, `WipeTower2.cpp`, `ToolOrdering.*`); out of scope for AMS-exclusive
  firmware-handshake sequences that only run on Bambu hardware.
- **Post-processing G-code** (thumbnails, cancel objects, timelapse triggers): In scope — these directly affect
  print initiation and host compatibility regardless of printer brand.
- **Firmware-specific WiFi upload bugs**: In scope if it affects the OctoPrint, Moonraker/Klipper, or Flashforge
  protocol paths; out of scope if the bug is exclusively Bambu X1/P1/A1 firmware behavior.

---

## When to Run a Triage Pass

Trigger a triage pass when **any** of these occur:

| Trigger | Action |
|---|---|
| Upstream cuts a new minor version | Full triage pass — search new PRs merged since last pass |
| A print failure traceable to a known upstream issue | Targeted search on that issue only |
| A specific feature is wanted before the next print run | Targeted search for PRs/issues matching that feature |
| Security advisory for a bundled dependency | Immediate: check `deps/` and update |
| More than ~90 days since last pass | Light pass: scan merged PRs and new open bugs |

Do **not** run a triage pass on a fixed calendar schedule if nothing has changed. That wastes tokens re-reading
already-triaged material.

---

## Discovery: How to Search (Token-Efficient)

**Important context:** SoftFever/OrcaSlicer merges community engine contributions at a much higher rate than
PrusaSlicer does. That means the single highest-value, lowest-cost action is a routine sync against upstream `main`
— most real engine improvements are already sitting there waiting to be pulled in, not stuck in a rejected-PR
graveyard. Weight searches accordingly: syncing beats mining.

Run these searches in the default order below, **except:** when the trigger was a specific print failure or
known bug, jump directly to step 3 (high-confidence bugs) and run steps 1-2 after.

### 1. Merged upstream commits/PRs not yet synced (highest yield, lowest cost)
```
- git fetch upstream && git log HEAD..upstream/main --oneline
- Filter: commits that don't touch localization/i18n/ or generated headers
```
This is the primary channel for engine improvements on this fork — a clean merge captures work upstream already
reviewed and accepted. Expect a genuine mix of profile additions *and* real engine/feature fixes, not just profiles.
Resolve conflicts per the Adapt tier below.

### 2. Security dependency updates
```
- Check deps/*/ cmake files: compare version strings against known EOL dates
- CVE databases for bundled libs: OpenSSL, CURL, libpng, zlib, Boost, wxWidgets
```
Always act on these. Dependency upgrades with no API break cost almost nothing.

### 3. High-confidence bugs in scope
```
- Open issues labeled "bug" with a stacktrace or clear repro steps
- Filter: affects FDM, G-code output, or print-host integration
```
Read title + first comment only. If scope isn't clear from those, skip to the next item.
Reporter activity is irrelevant — a silent issue with a clear repro is still actionable.
For old issues (>1 year), verify the repro still applies to current HEAD before committing to implement.

### 4. Open PRs not yet merged upstream
```
- Open PRs on SoftFever/OrcaSlicer, sorted by most recently updated
- Filter: files changed don't touch localization/i18n, or generated headers
- Filter: not a draft
```
Given upstream's PR volume, good fixes can sit unmerged for weeks — this is still worth scanning, just don't assume
"open and unmerged" implies upstream rejected it (unlike the PrusaSlicer case, silence here is far more likely to
just mean maintainer backlog). Apply antipatterns; decompose bundles. Verify conflict-freedom at implementation
start, not at triage time.

### 5. Closed-without-merge PRs worth resurrecting
```
- Closed PRs where close reason is NOT "superseded by X" or "duplicate of X"
- Filter: fits hardware scope above
```
Less common here than on a stagnant fork, but still happens — process reasons (no tests, author inactive, doesn't
fit maintainer's roadmap) can sink a good fix regardless of how active a project is. If a closed PR was reopened as
a new number, track the live one; treat the closed one as historical context only.

### 6. Everything else
```
- Feature requests, enhancements, cosmetic changes, low-comment bugs
```
Score with: **Impact on print quality or workflow ÷ Implementation effort**. Upstream user count is not a factor —
we care whether *this* user's printing is improved, not aggregate adoption.
When in doubt, skip. Missing a low-priority item costs nothing; implementing a low-value item wastes budget.

---

## Blast Radius Assessment

Before implementing any change — upgrade, bug fix, or feature — trace one level in each direction:

- **Callers**: what calls the modified function, class, or interface? If the answer is "many unrelated systems,"
  the change touches a shared contract. Verify each caller's assumptions are still satisfied.
- **Callees**: what does the modified code depend on? If it assumes an invariant about its dependencies, verify
  that invariant still holds after the change.
- **Data flow**: if the change affects a data structure or config value, trace where that value is read and whether
  any reader assumes a range, format, or ordering the change might violate.

This is not a skip criterion — it's a scoping exercise. A change with a large blast radius is not automatically
dangerous; it depends on whether the affected callers are actually sensitive to what changed.

**If blast radius is large due to tight coupling** — many callers depending directly on implementation details
rather than an interface — that is an architectural observation worth noting in CHANGELOG.md or a TODO comment.
It does not block the change, but it explains why the change feels riskier than it should be. Well-designed code
has bounded blast radius by construction; when it doesn't, the coupling itself is the bug.

**For dependency upgrades specifically:** before updating a library version, grep for every call site that uses
its API. For each: does the new version change the behavior of that function, deprecate it, or alter its error
semantics? If yes, fix the call site. If behavior is unchanged and only the URL/hash changes, the blast radius
is zero and no further analysis is needed.

**Conservative implementation when blast radius is genuinely wide:** if a change must touch a widely-shared
interface, prefer the narrowest possible diff that achieves the goal. Don't refactor adjacent code while fixing
the bug. Narrow changes have narrow blast radius even in tightly-coupled systems.

## Evaluating a PR: The Diff Is the Primary Artifact

When assessing a PR, read the diff first — not the description. The description may be absent, vague, or
misleading; the code is what actually gets applied. Claude can infer the problem being solved from a clear
fix. A well-implemented change is self-documenting through the code itself.

**Positive signals (read from the diff, not the description):**
- The change is locally comprehensible — reading the modified functions is sufficient to understand what changed and why
- The fix addresses an obvious code-level problem (wrong condition, missing guard, incorrect formula)
- The scope is coherent — the files touched form a logical unit around one concept
- Where behavior changes, the new behavior is clearly the correct one

**Negative signals (also read from the diff):**
- The change introduces variables, flags, or branches whose purpose cannot be inferred from the surrounding code
- New code relies on invariants that aren't enforced or documented anywhere visible
- The diff touches many unrelated files with no apparent connecting logic (possible bundle — assess decomposability)
- The implementation contradicts or ignores existing patterns in the same file without explanation

A PR with no description but a clean, readable diff is more valuable than a PR with a detailed write-up
and tangled implementation. When the diff is clear, don't spend tokens trying to recover intent from the
description — the code already told you.

## The Decision Gate

**Fast lane — security deps:** If the item is a CVE or EOL notice for a bundled library and the upgrade is
API-compatible (no source changes required), skip the full gate and go directly to Security-patch tier.
If the upgrade breaks API, enter at step 4 with Impact=3.

**Major version jumps** (e.g., 1.x → 3.x, 7.x → 8.x): read the library's migration guide or release notes
before applying. Grep call sites in this codebase for any APIs that were removed or changed. If any are found,
treat as Adapt rather than Security-patch and resolve each call site before proceeding.

For everything else, answer these in order. Stop at the first "skip":

```
1. Is it in hardware scope?                                    NO  → skip
2. Does it hit an antipattern? (see below)                     YES → skip
3. For bugs: is the repro verifiable against current HEAD?     NO  → skip (old issues may be already fixed)
4. Does Impact ÷ Effort justify the work?                      NO  → skip or defer
5. Does a PR or patch already exist?                           YES → cherry-pick/adapt tier (read the diff)
                                                               NO  → implement tier
6. Verify conflict-freedom at implementation start (not here). Conflicts found → adapt tier.
   Mechanical conflicts (renames, offsets) → adapt. Semantic conflicts (logic changed) → re-evaluate from step 1.
```

Note on step 5: on this fork, "PR still open" is more often a backlog artifact than a rejection — SoftFever/OrcaSlicer
merges readily, so lingering PRs are weaker negative signal here than they'd be on a slower-moving upstream.

**Upstream wontfix override at gate 4:** If the item was closed upstream for a reason we explicitly override
("has a workaround," "functionally redundant," "not a Bambu printer issue," "too niche") and Impact ≥ 2,
treat gate 4 as passed regardless of the Impact − Effort score. The rationale: upstream's cost calculus
doesn't apply to us, so their skip reasons can't be used to price the work down.

**Age flag for old PRs:** PRs older than 2 years should be flagged before classification. Do not assume
the patch applies cleanly — attempt a diff against current HEAD first. If it no longer applies, downgrade
to Adapt or Implement tier rather than Cherry-pick.

### Impact ÷ Effort scoring

Score each axis 0–3. **Take if Impact − Effort ≥ 1.**

**Impact = 3 always warrants action, but requires evidence before starting:** at least one of — a stacktrace,
a repro verifiable against current HEAD, or a concrete failing input. Without any of these, route to
Investigate+Implement rather than Implement directly. "This sounds like it causes crashes" is not evidence.

| Score | Impact | Effort |
|---|---|---|
| 3 | Causes print failure, crash, or invalid G-code | Investigation required; root cause unknown; multi-file entangled |
| 2 | Visibly degrades print quality or major workflow friction | Moderate diff; some coupling; 2–5 files |
| 1 | UX convenience; minor friction | Localized single-file change |
| 0 | Cosmetic; no behavioral change | Trivial (URL/hash bump, one-liner) |

Examples: crash fix with clear root cause = Impact 3, Effort 1 → take. Cosmetic UI change = Impact 0, Effort 1 → skip.
Feature touching perimeter generator + supports + config = Impact 2, Effort 3 → skip (−1) unless it uniquely
matters to owned hardware.

---

## Antipatterns — Skip Even If It Looks Good

These patterns indicate an issue or PR that will cost more than it delivers. Skip them regardless of vote count,
upstream merge status, or apparent simplicity.

### Scope antipatterns
- **SLA-only**: Touches only `libslic3r/SLAPrint*`, `libslic3r/SLAPrintSteps*`, or
  `src/slic3r/GUI/Gizmos/GLGizmoSlaSupports*`. Zero value here.
- **Bambu-cloud only**: Touches only Bambu Cloud, Bambu Handy app integration, or account/store features in
  `BBLCloudServiceAgent`/`bambu_networking` beyond generic print-submission plumbing. We don't own Bambu hardware.
- **macOS/iOS only**: Issue is only reproducible on macOS and the fix touches only Apple-specific code. Not worth
  the risk of breaking something on Windows for a platform we don't test.
- **Wayland-only**: Linux Wayland support. Not in scope.

### Conflict-surface antipatterns

These are not about whether AI can maintain the code — Claude can maintain well-structured C++ indefinitely.
They are about whether the change will cause perpetual merge conflicts with upstream, which is a *different*
cost paid on every future sync.

- **Global rename / mass refactor**: PR renames types, methods, or namespaces across 20+ files. Even if upstream
  merged it cleanly, cherry-picking it means every future upstream diff lands on a different symbol name. Defer
  until upstream's rename has fully propagated and a clean sync is possible.
- **Touches localization strings**: Any PR adding, changing, or moving translatable strings in `localization/i18n/`
  or adding new `_L(...)` / `_u8L(...)` calls. Localization is upstream-managed. String changes create perpetual
  conflicts with every upstream translation update.
- **Changes generated or binary assets**: Touches auto-generated source files or binary resources (icons, presets).
  These regenerate on every upstream build and will conflict immediately.
- **"Prepares for future work" with no deliverable**: Refactor whose sole purpose is to enable a future PR that
  hasn't arrived yet. Zero current value, creates a growing diff surface. Skip until the payoff PR exists.

### What Claude can and cannot realistically maintain

This replaces the old human-bandwidth definition of "maintainability."

**Claude can maintain indefinitely:**
- Any well-structured C++ with clear data flow and named interfaces
- Abstractions with self-evident purpose — a new class or layer is fine if its invariants are visible in its
  interface and don't require reading the full codebase to understand
- Code whose behavior is derivable from reading the code itself, without relying on external context that
  isn't captured in comments or naming
- Fixes to isolated subsystems (Arachne, extruder math, thumbnail pipeline, etc.) that have clear input/output
  contracts

**Claude needs help with:**
- Code with hidden invariants — magic numbers, load-bearing ordering dependencies, or assumptions that are
  nowhere in the source (e.g., "this must be called before X or the global state is wrong")
- Threading code where correctness depends on memory ordering that isn't enforced by the type system
- Large entangled subsystems where a change in one place silently affects behavior three layers away with no
  visible connection
- Code that requires running the real hardware to verify (no unit test possible)

**Practical implication:** "Adds a new abstraction layer" is no longer an antipattern in itself. The question
is whether the abstraction is *well-designed*. A clean factory or manager class that encapsulates a real concept
is maintainable by Claude. An abstraction that exists to paper over technical debt without resolving it is not.

### Upstream "wontfix" decisions we do not inherit

Upstream closes issues for reasons that do not apply to this fork. Do not automatically follow their closure:

- **"Has a workaround"**: Workarounds are recurring friction. If the root cause is fixable, fix it regardless of
  whether a workaround exists. An upstream close reason of "use setting X instead" means nothing here.
- **"Functionally redundant"**: Upstream may close a UI feature as redundant because the value is derivable from
  other settings. Derivable ≠ convenient. Evaluate on usability merit, not on whether the math works out.
- **"Not a Bambu printer issue"**: Upstream may close bugs that only affect third-party or custom printer profiles.
  For this fork those are the primary use case — treat these as high-priority bugs.
- **"Too niche / affects too few users"**: Upstream thinks in aggregate user counts. We think in print quality.
  If the bug affects this user's workflow, user count is irrelevant.
- **"Feature not aligned with our roadmap"**: Upstream may decline features that don't fit their product direction.
  This fork has no such roadmap constraint.

**Exception — intentional removals:** If upstream *removed* a feature (not merely declined to add it), treat
reimplementation as an Implement-from-scratch effort. Find why it was removed before proceeding — the removal
may have been for correctness, stability, or licensing reasons that still apply.

### Security-surface antipatterns
- **New networking or IPC endpoint**: Any PR adding a listening socket, localhost HTTP server, named pipe,
  or other inbound communication channel — even opt-in — requires explicit security review before TAKE.
  The opt-in nature does not eliminate the attack surface once enabled; it just reduces exposure. Read the
  networking code, assess what can reach it and what it can do, then decide.

### Competitor port antipatterns
- **Feature ported from a competing slicer without license audit**: Ports from PrusaSlicer, SuperSlicer, Cura,
  etc. require a license compatibility check before any other evaluation. These projects have different licenses
  (AGPL, LGPL, proprietary); porting their code without confirming compatibility could introduce license obligations.
  Check license first; if compatible, evaluate normally.
  **Hard skip:** AGPL or GPL code ported into this codebase (which is AGPL itself, but copyleft obligations
  on incorporated code still need verification). If the source project is proprietary or has incompatible
  additional restrictions, skip without further evaluation.
- **BambuStudio-attributed features specifically**: OrcaSlicer's own lineage runs through BambuStudio, but Bambu
  Lab has a documented history of delaying or withholding source releases for newer BambuStudio versions relative
  to the shipped binary. Before treating any "ported from BambuStudio" claim as AGPL-clean, verify that Bambu Lab
  actually published matching source for that feature's version. If no matching source release exists, treat the
  feature as if it came from a closed-source competitor and skip rather than reimplement from binary observation.

### Behavior-tightening antipatterns
- **Parser or protocol change that increases strictness**: A change that makes a previously permissive behavior
  strict (e.g., case-sensitive G-code matching, stricter config validation, rejecting previously-accepted input)
  is a regression risk even if logically correct. Require evidence that the stricter behavior does not break
  real-world G-code files or firmware variants before TAKE. "Spec says it should be strict" is not sufficient.
- **Validation limit relaxation**: A change that widens acceptable input ranges (e.g., relaxing extrusion
  multiplier bounds, expanding config value limits) carries the same evidence requirement as tightening — just
  in the other direction. Require a reproducible case where the current limit causes a real failure. "The limit
  seems conservative" is not sufficient; conservative limits often exist because someone got burned.

### Dependency antipatterns
- **New external library via FetchContent or bundled source**: Adding a new dependency means a deps rebuild on
  every machine and a new maintenance surface. Before TAKE: check that the library is actively maintained, has a
  compatible license, and that the feature benefit justifies the permanent build cost. A single-purpose library
  for a niche feature is a higher bar than a well-established utility library.
  **Hard skip:** Libraries with no commits in the last 5 years. An unmaintained library is a future CVE
  waiting to happen and will never be updated by the community. Don't add it regardless of feature value.

### Draft PR rule
- **Draft PRs**: Skip at triage time. Add a note to revisit when draft status is lifted. Do not implement
  from a draft — the author is signaling the work is incomplete.
- **WIP without draft flag**: PRs with no description AND a title indicating work-in-progress ("wip",
  "still not", "testing", "temp", "some X working") → treat as MONITOR regardless of draft flag.
  GitHub's draft state is opt-in; authors don't always use it.

### Quality antipatterns
- **No repro, no stacktrace, and low importance**: A crash report with no steps, no stack trace, and no reason
  to think it affects this fork's use cases. Skip. If the crash *does* seem relevant, Claude can investigate
  independently — reporter activity is not required (see Investigation tier below).
- **Works on my machine only, and the machine is irrelevant**: Reporter's environment is the only known context
  and it's a platform or config this fork doesn't use. Skip. If the environment could be relevant (custom printer,
  third-party host), it's worth a closer look regardless of whether the reporter responds.
- **Fix-by-disable**: The proposed fix works by disabling a feature or defaulting to a less capable path
  (e.g., "just set this config flag to false"). These mask root causes and create hidden behavioral regressions.
- **Thread safety fix without test**: Any PR touching threading, parallel TBB loops, or shared mutable state
  without an accompanying reproduction test. The blast radius of a wrong threading fix is severe and silent.
- **Large OpenSSL/CURL API migration with no test coverage**: Upgrading a crypto dependency where the fix
  touches `EVP_*`, TLS config, or cert validation paths without any test that exercises the changed code path.
  (Simple URL/hash dep upgrades with no API change are fine — this antipattern is about *code* changes in the
  TLS stack.) Relevant right now: `deps/OpenSSL`, `deps/CURL`, `deps/Boost`, and `deps/TBB` all currently show
  local modifications — apply this check to whichever of those is a real API-affecting change versus a pure bump.

### Cost antipatterns
- **Investigation required + low importance**: Root cause unknown AND the issue doesn't affect active printing.
  The fix is not known, the reporter isn't providing more info, and there's no reason to prioritize it now.
  Defer. If it *does* affect active printing, move it to the Investigate tier — Claude can chase it down.
- **"Just needs a bit of cleanup first"**: A PR requiring another fix before it can be applied. Cascading
  dependencies multiply token cost unpredictably. Break the dependency chain: take each item separately.
- **Genuinely monolithic PR with no verification path**: A large PR where all changes are interdependent (cannot
  be decomposed) AND correctness can only be verified by examining physical print output. The issue is not size
  or blast radius — it's the absence of any way to confirm correctness short of printing. Flag for manual
  print-test verification after implementation; do not skip on this basis alone.

---

## PR Size and Decomposition

PR size is not a skip criterion. It is a signal to assess decomposability before integration.

**Bundle** — changes that happen to be in one PR but are logically independent:
- Different subsystems modified for unrelated reasons
- Refactor + bug fix in the same PR
- Multiple feature improvements grouped by the author for convenience
→ **Decompose.** Apply each independent piece separately. Each piece is evaluated on its own merits.
→ **Regret rule:** When decomposing a bundle, take the highest-impact piece first. If tokens run out mid-bundle,
  the most valuable work is already done.

**Monolith** — changes that are genuinely interdependent:
- A new data structure plus all the code that uses it
- A pipeline refactor where every stage was rewritten together
- A multi-file fix where the invariant spans all the files
→ **Evaluate as a unit.** Read the whole diff. Decide whether the complete change is worth taking.
   If the monolith touches a subsystem with no verification path, flag it for a print-test before committing.

**Soft size threshold:** If the diff exceeds ~800 lines changed, attempt decomposition before proceeding. If
the diff exceeds ~1500 lines and no seam is found, defer pending a human review of the approach.

**Practical rule:** Start reading the diff. If you can identify a clean seam where one part can be applied
without the other, it's a bundle. If every change depends on every other change, it's a monolith.

## Outcome Tiers

| Tier | Condition | Token Cost | Action |
|---|---|---|---|
| **DONE** | Already applied in this fork | Near-zero | Verify fix still exists in current HEAD; then skip |
| **Security-patch** | CVE or EOL for bundled dep; API-compatible upgrade | Low | Update URL + SHA256 in deps cmake; record |
| **Cherry-pick** | PR exists, no conflict, no antipattern | Low | Apply diff; verify compiles |
| **Adapt** | PR exists, minor conflicts | Low-medium | Resolve conflicts, apply |
| **Implement** | No PR; clear root cause and bounded scope | Medium | Write fix directly |
| **Investigate + Implement** | Root cause unknown; Impact ≥ 2 | High | Bound the investigation; record what was ruled out |
| **Defer** | Valid, in scope, but blocked or lower priority than active work | Zero now | Record reason + re-check condition (e.g., date, upstream event) |
| **Monitor** | Draft PR or early-stage work on a high-value feature | Zero now | Note to revisit when draft is lifted |
| **Skip** | Antipattern, out of scope, or Impact − Effort < 1 | Zero | Record reason; move on |

**DONE verification:** Run a quick search (`git log --grep=<issue#>` or grep the relevant fix) to confirm the
change still exists in HEAD. Upstream rebases and our own refactors can silently drop a cherry-pick. If the fix
is no longer present, re-enter the gate as a Cherry-pick.

**Defer vs Skip:** Skip means "this is not worth doing." Defer means "this is worth doing but not now" — because
something it depends on isn't ready, or a higher-priority item takes precedence. Deferred items must have a
re-check condition, not just a date. "Re-check when INDX hardware arrives" is better than "re-check 2027-01-01."

For **Cherry-pick** and **Adapt** tiers: read only the diff and the directly touched files. Do not read the full
issue thread.

For **Implement** tier: read the minimal set of files needed to understand the data flow. Stop reading when the
fix is clear.

For **Investigate** tier: before starting, write down what evidence would constitute "root cause found" and
what the main hypotheses are. Default scope: read up to 3 files or eliminate the main hypotheses, whichever
comes first. Stop there. If unresolved, mark the TODO.md entry as `investigate-blocked`, record what was ruled
out and what remains unclear, and exit. Do not continue into open-ended exploration. Partial findings are more
useful than an exhausted budget with no conclusion.

## Known Risky Subsystems

Changes to the following subsystems require a full blast-radius trace even if the diff is small. These are
areas where a small change in the wrong place can produce subtly wrong prints that are hard to detect:

- **Arachne perimeter generation** (`libslic3r/Arachne/`) — wall width math, bead ordering, trapezoidation
- **Support material** (`libslic3r/Support/` — `SupportMaterial.cpp`, `TreeSupport*.cpp`, `TreeModelVolumes.cpp`,
  `SupportCommon.cpp`) — contact layer Z, bridge detection
- **Retraction and wipe transitions** (`libslic3r/Extruder.cpp`, `libslic3r/GCodeWriter.cpp`) — E-value accounting
- **Multi-tool sequencing** (`libslic3r/GCode/ToolOrdering.*`, `libslic3r/GCode/WipeTower.*`,
  `libslic3r/GCode/WipeTower2.*` — this fork carries two wipe tower implementations, check which is active for
  the affected profile) — tool-change handoff correctness
- **G-code exporter** (`libslic3r/GCode.cpp`) — emitted G-code structure, layer markers, toolchange order

For changes in these subsystems, apply the minimal diff, then slice a canonical test model and inspect the
relevant portion of the G-code output before recording completion.

---

## After Implementation

**Verification checklist (minimum):**
1. Build succeeds for the current target platform.
2. No new compiler warnings in the modified files.
3. If G-code output is affected: slice a representative model with the same profile and confirm the output
   structure is sane (layer count unchanged, no missing end-gcode, E-values advance monotonically).
4. If a config setting was added or changed: toggle the setting and confirm the app doesn't crash and the
   value persists across restart.
5. If a dependency URL/hash was updated: confirm the build system accepts the new hash (download succeeds).

For known risky subsystems (above): additionally inspect the relevant G-code section around the changed path.

**Record-keeping:**
- Mark the item `[x]` in `TODO.md`
- Add an entry to `CHANGELOG.md` using the format:
  ```
  - **[What changed]** (upstream #NNNN or issue #NNNN)
    One-sentence description of what was wrong and what the fix does.
    Files: `path/to/file.cpp`, `path/to/file.hpp`
  ```
- If the fix is a good upstream contribution candidate, note it in TODO.md as `upstream/candidate`
- Do not commit unless explicitly asked

---

## Delegating to a Local Code Agent

Triage and judgment are expensive — they require the full policy context, fork history, and hardware scope.
Applying a well-understood, fully-specified change is cheap and can be delegated to a capable local code model
(e.g., Qwen 3 Coder 35B via Ollama, Aider, Continue.dev, or similar — no API key required).

**Keep in the senior agent (needs context):**
- Triage pass: reading PRs, applying antipatterns, scoring Impact ÷ Effort
- Blast radius assessment: requires knowing the fork's divergence points
- Investigation: open-ended root cause search
- Any decision involving wontfix overrides or intentional-removal rules
- Any task where the brief would require more than ~5 sentences to fully specify

**Good delegation candidates (execution only):**
- Applying a clean cherry-pick where the exact file, function, and change are identified
- Updating a dependency URL + SHA256 in a cmake file
- Applying a batch of spelling/typo fixes across a list of specified files
- Verifying a build and reporting warnings after a change is described

**How to write a delegation brief:**

A local code agent has no knowledge of this fork, the triage policy, or why the task matters. It is strong
at C++ editing when fully specified; it struggles with ambiguity and multi-file reasoning in an unfamiliar
codebase. A good brief removes all judgment from the task:

1. **File and location**: exact path, function name, and line context.
2. **The change**: paste the relevant diff or describe precisely in terms of before/after.
3. **Done condition**: what output to produce ("compiles without new warnings" or "changed lines X and Y").
4. **Scope boundary**: describe what's in scope and what's not. Local models tend to over-help; without
   an explicit boundary they may attempt to "improve" surrounding code in ways that conflict with the fork.

Example brief:
> File: `src/libslic3r/Extruder.cpp`, function `Extruder::retract()`.
> Change this line:
>   `if (to_retract > 0.f) this->m_restart_extra = restart_extra;`
> to:
>   `this->m_restart_extra = restart_extra;`
> That is the complete change. Do not modify surrounding code — this codebase has invariants
> you're not aware of. Report the before and after lines only.

**Calibrate brief length to model capability.** Qwen 3 Coder 35B follows precise, mechanical instructions
well. It does not reliably infer missing context from surrounding code in an unfamiliar large codebase. If
specifying the task requires explaining what the codebase does, what the invariants are, or why the change
is correct — that context overhead means the task is not ready to delegate. Spend the senior-agent tokens
to fully specify it first, then delegate.

**When NOT to delegate:** If the task requires reading this policy, understanding fork divergence, or making
any scope or priority judgment, keep it in the senior agent. Delegation saves cost only when the task is
fully specified before handoff. An under-specified task costs more to delegate than to do directly.

---

## What This Policy Does Not Cover

- How to rebuild deps after an upgrade (see build docs)
- How to write tests (no test framework policy established yet)
- How to handle merge conflicts with upstream syncs (future topic)
- SLA-specific issues (out of scope for this fork)
