Reconcile the environment files.

- Compare the keys in `.env` against the keys in `.env.example`.
- Report keys present in `.env.example` but missing from `.env` (need a value).
- Report keys present in `.env` but missing from `.env.example` (should be documented).
- Suggest the lines to add to `.env.example` (key names only, never real values).
- Do not print the actual secret values from `.env`.
