Review the current uncommitted changes and commit them.

- Run `git status` and `git diff` to see what changed.
- Group changes into small, logical commits — do not bundle unrelated work.
- Write each message as a Conventional Commit: `type(scope): summary`
  (types: feat, fix, docs, style, refactor, perf, test, build, ci, chore).
- Imperative summary, under ~72 chars, no trailing period. Add a body for the "why" when useful.
- Do not stage `.env`, secrets, build output, or `node_modules`.
- Do not push unless I ask.
