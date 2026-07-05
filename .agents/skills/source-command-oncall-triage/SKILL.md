---
name: "source-command-oncall-triage"
description: "Triage GitHub issues and PRs and label critical ones for oncall"
---

# source-command-oncall-triage

Use this skill when the user asks to run the migrated source command `oncall-triage`.

## Command Template

This runs as two independent passes over GitHub activity for this repo:

- **Pass 1 — Oncall labeling** (Parts A/B below): narrow and conservative. Only things that
  are genuinely *blocking* get the "oncall" label — that label is a signal to a human that
  something needs urgent attention, so a false positive here has a real cost.
- **Pass 2 — Roadmap capture** (Part C below): a broad net. Valuable feature requests and
  enhancements matter just as much as bugs for this repo's backlog — don't limit this pass to
  conservative bugfixes only. Nothing in Pass 2 needs the "oncall" label; it just needs to end
  up recorded so it isn't rediscovered from scratch on the next triage run.

Repository: OrcaSlicer/OrcaSlicer (the public upstream project) — **not necessarily this
checkout's git `origin`.** Check both before doing anything else.

**Permission preflight (do this first, once):** GitHub label writes require push/triage
access on the target repo — read access (which is all a public repo guarantees) is not
enough, and `gh issue edit --add-label` fails or silently targets the wrong project
otherwise.
```bash
gh api repos/OrcaSlicer/OrcaSlicer --jq '.permissions'
```
- If `push` or `triage` is `true`: the labeling sub-steps (4a in Parts A/B) are live. Confirm
  the "oncall" label exists first (`gh label list --repo OrcaSlicer/OrcaSlicer --search
  oncall`); create it with `gh label create` if missing, don't assume it's already there.
- If both are `false` (the common case when this repo's `origin` is a personal fork rather
  than the upstream project itself — compare against `git remote -v`): **skip every 4a
  labeling sub-step below** and rely entirely on the roadmap-capture sub-steps (4b). Say so
  explicitly in the wrap-up rather than silently attempting writes that will fail. Do not
  redirect labeling to a different repo (e.g. your own fork) without asking first — a fork
  doesn't share the upstream project's issues/PRs, so labeling there wouldn't mean the same
  thing.

**Backlog file check (do this once too):** Look for `docs/roadmap.md` in this repo (the local
checkout, not upstream). If it exists, that file — not a fresh TODO file — is the single
source of truth for backlog items; read its "Format convention" and "Scoring" sections so new
entries match its existing shape (header line with Score/Class, then `Context:`/`Implement:`
file-list fields), and read `TRIAGE_POLICY.md` if present for how to score Impact/Effort and
apply hardware scope. If no backlog file exists, skip the backlog-integration steps below
(4b, and all of Part C) — do not invent a new TODO file to hold findings.

Before appending anything, skim the existing roadmap sections (including "PR Review Queue"
and the "Notes" section listing intentional exclusions) so you don't re-add an item that's
already tracked or was deliberately left out.

Use the TodoWrite tool to track your own progress through the issue/PR lists below (so you
don't lose your place mid-run) — that list is session-scoped bookkeeping, not a deliverable.

**Tooling note:** the `jq` filters below assume `jq` is installed and that `gh`'s reaction
field is named `reactionGroups` (each group has a `users.totalCount`, not a flat `reactions`
array — an older assumption in this file undercounted engagement). If `jq` isn't on PATH,
the same filter logic works via PowerShell's `ConvertFrom-Json` — pull the day's fields with
`--json`, save to a temp file if the payload is large, then filter with `Where-Object`.

### Part A — Issues (oncall label)

1. Get all open bugs updated in the last 12 months with at least 3 engagements (broad
   enough to not miss slow-burn threads, since oncall status is decided by content, not
   volume):
   ```bash
   gh issue list --repo OrcaSlicer/OrcaSlicer --state open --label bug --limit 1000 --json number,title,updatedAt,comments,reactionGroups | jq -r '.[] | select((.updatedAt >= (now - 31536000 | strftime("%Y-%m-%dT%H:%M:%SZ"))) and ((.comments | length) + ([.reactionGroups[].users.totalCount] | add // 0) >= 3)) | "\(.number)"'
   ```

2. Add every number to your TodoWrite list so you process each one.

3. For each issue:
   - Use `gh issue view <number> --repo OrcaSlicer/OrcaSlicer --json title,body,labels,comments` to get full details
   - Read and understand the full issue content and comments to determine actual user impact
   - Evaluate: is this truly blocking users?
     - Consider: "crash", "stuck", "frozen", "hang", "unresponsive", "cannot use", "blocked", "broken"
     - Does it prevent core functionality? Can users work around it?
   - Be conservative — only flag issues that truly prevent users from getting work done

4a. For issues that are truly blocking and don't already have the "oncall" label:
   - Use `gh issue edit <number> --repo OrcaSlicer/OrcaSlicer --add-label "oncall"`
   - Mark the issue complete in your TodoWrite list

4b. If a backlog file exists (see check above) and this issue isn't already represented in
   it, append an entry in the file's existing format under the matching severity section
   (Critical for crash/data-loss, High for other blocking bugs). Don't guess a `Route:`,
   `Effort:`, or `Chars:` value — leave those off; a routing script regenerates them.

### Part B — Pull requests (oncall label)

1. Get open, non-draft PRs updated in the last 12 months with at least 3 engagements:
   ```bash
   gh pr list --repo OrcaSlicer/OrcaSlicer --state open --limit 1000 --json number,title,updatedAt,comments,reactionGroups,isDraft,body | jq -r '.[] | select(.isDraft == false and (.updatedAt >= (now - 31536000 | strftime("%Y-%m-%dT%H:%M:%SZ"))) and ((.comments | length) + ([.reactionGroups[].users.totalCount] | add // 0) >= 3)) | "\(.number)"'
   ```

2. Add every number to your TodoWrite list.

3. For each PR:
   - Use `gh pr view <number> --repo OrcaSlicer/OrcaSlicer --json title,body,labels,comments,files` to get full details
   - Read the diff description and file list (read the actual diff with `gh pr diff <number>` if scope is unclear from the summary alone)
   - Evaluate: does this PR fix a blocking bug — a crash, hang, data-loss, or "cannot use"
     issue — either described in its own body or via a "Fixes #N"/"Closes #N" reference to an
     issue that meets Part A's blocking criteria?
   - Be conservative — a PR that only touches cosmetic/UX issues, or one still marked WIP in
     its title/body without the draft flag, does not qualify

4a. For PRs that qualify and don't already have the "oncall" label:
   - Use `gh pr edit <number> --repo OrcaSlicer/OrcaSlicer --add-label "oncall"`
   - Mark the PR complete in your TodoWrite list

4b. If a backlog file exists and this PR isn't already represented in it, append an entry
   under the file's PR review queue section (or create one, following the same header/
   file-list format as the issue entries) rather than a new file.

### Part C — Feature requests and enhancements (roadmap capture only, broad net)

Skip this part entirely if no backlog file exists (see check above) — it has no GitHub-side
output, so there's nowhere to put its findings without one.

1. Get open, non-bug issues with meaningful engagement, no time cutoff (demand for a feature
   doesn't decay the way a live bug report does):
   ```bash
   gh issue list --repo OrcaSlicer/OrcaSlicer --state open --limit 1000 --json number,title,updatedAt,comments,reactionGroups,labels | jq -r '.[] | select(([.labels[].name] | index("bug") | not) and ((.comments | length) + ([.reactionGroups[].users.totalCount] | add // 0) >= 10)) | "\(.number)"'
   ```

2. Add every number to your TodoWrite list.

3. For each issue, apply `TRIAGE_POLICY.md`'s Decision Gate: hardware scope, antipatterns,
   Impact ÷ Effort, and whether a PR already exists for it. This is deliberately not
   conservative the way Parts A/B are — a well-loved, in-scope feature request belongs on the
   roadmap even though it isn't oncall material. Skip only what the policy says to skip
   (out of scope, antipattern, or Impact − Effort < 1).

4. For every issue that passes the gate and isn't already in the backlog file, append an
   entry in the matching severity/priority section, in the file's existing format (same
   header shape as Parts A/B — `Context:`/`Implement:` file lists, no hand-typed `Route:`/
   `Effort:`/`Chars:`).

### Wrap-up

5. After processing everything, provide a summary:
   - List each issue/PR number that received the "oncall" label, its title, and the reason it qualified
   - List every backlog-file entry you added (Parts A/B/C), and which section it landed in
   - If nothing qualified in a category, state that clearly

Important:
- Process every issue and PR in your TodoWrite list systematically
- Don't post any comments to issues or PRs
- Only add the "oncall" label, never remove it — and only for Parts A/B, never for Part C
- Use individual `gh issue view`/`gh pr view` commands instead of bash for loops to avoid approval prompts
