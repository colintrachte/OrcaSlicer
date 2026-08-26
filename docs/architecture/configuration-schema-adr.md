# ADR: Configuration schema — shadow-generate, do not rewrite

**Status:** Accepted; closes the Phase 3 exit gate in `research/orcaslicer-improvement-plan.md`
**Date:** 2026-08-25
**Decision scope:** Whether and how OrcaSlicer's hand-registered print/printer/filament option
definitions should move toward a generated schema
**Validation scope:** Windows x64; macOS/Linux validation deferred by user, per the rest of this
plan

## Context

Every print/printer/filament option is registered imperatively in `PrintConfig.cpp` (12,965
lines) via a builder pattern:

```cpp
def = this->add("wall_loops", coInt);
def->label = L("Wall loops");
def->category = L("Strength");
def->tooltip = L("This is the number of walls per layer.");
def->min = 0;
def->max = 1000;
def->set_default_value(new ConfigOptionInt(2));
```

`Config.hpp`/`Config.cpp` (3,089 + 2,136 lines) supply the generic `ConfigOptionDef`/`ConfigDef`
machinery, serialization, and mutation APIs this pattern builds on. C2 (Phase 1) added a read-only
oracle, `normalized_config_registry_json()` (`ConfigRegistry.cpp`), that introspects the
already-constructed registry into a deterministic, nonlocalized JSON view — not a second source of
truth, a verifier for one. The current Windows baseline is 759 sorted options at digest
`0x4fe1dfc4053e23d5`.

Registration is not uniform. Across the 759 options:

| Special-case field | Options using it | Share |
| --- | ---: | ---: |
| `enum_keys_map` (named enum values) | 68 | 9% |
| custom `cli_args` | 60 | 8% |
| `ratio_over` (percent-of-another-option) | 38 | 5% |
| `gui_type`/`readonly` special casing | 31 | 4% |

Excluding overlap, roughly a quarter of options carry at least one of these idiosyncrasies; the
remaining majority (~560-600 options) are structurally uniform: type, label, category, tooltip,
mode, numeric bounds, default. This split is the central fact this ADR acts on.

This decision was deliberately deferred until now. Executive decision 6 in the plan requires
config work to produce a user-visible payoff — the read-only preset inspector and, per the
2026-08-25 re-audit, true in-place preset rename — before any broad schema conversion, and the
plan's own non-goals list "conversion of all settings to a schema" and "a preset-format rewrite"
as explicitly out of bounds for this initiative.

## Decision

Do not convert the configuration registry to a generated schema. Shadow-generate one low-risk
setting family instead, and only keep the generated path if it earns its place against C2's
oracle.

1. Select a pilot family from the structurally-uniform majority: plain type, no `enum_keys_map`,
   no `ratio_over`, no custom `cli_args`, no `gui_type`/`readonly` special casing, Class 2 (no
   G-code-emission consequence if briefly wrong during development).
2. Author that family's key/type/label/category/tooltip/mode/bounds/default as data (a small
   table — JSON, YAML, or a generator script's literal input; format is an implementation detail,
   not this decision), separate from `PrintConfig.cpp`.
3. Generate `ConfigOptionDef` registration calls from that data at build time or load time, run
   alongside the existing hand-written registration for the same family without removing it yet.
4. Diff the generated family's `normalized_option()` JSON against the hand-written family's, using
   C2's existing oracle. A mismatch in any field — including `serialization_ordinal`, which
   controls on-disk/wire order — blocks the pilot.
5. Only after an exact match across a full build/test cycle, delete the hand-written blocks for
   that family and keep the generated path as the new source of truth for it.
6. Leave every option with `enum_keys_map`, `ratio_over`, custom `cli_args`, or `gui_type`/
   `readonly` casing hand-registered. Do not generalize the generator to cover them until a
   second pilot family demonstrates the pattern is worth extending — Workstream C4 names this "one
   low-risk setting family," singular, on purpose.

This is a mechanical-generation pilot, not a schema-first redesign. `ConfigOptionDef`,
`ConfigDef`, serialization, and the on-disk JSON/INI formats are unchanged. Nothing about preset
files, `.3mf` projects, or the wire format changes; only how one family's definitions are
authored in source.

## Options considered

| Option | Coverage | Risk | Decision |
| --- | --- | --- | --- |
| Convert all 759 options to a generated schema in one pass | Complete | Very high — the ~25% with `enum_keys_map`/`ratio_over`/custom CLI/`gui_type` casing have no uniform shape to generate from; a generator that special-cases a quarter of its inputs is not earning its complexity budget | Reject — also an explicit plan non-goal |
| Leave `PrintConfig.cpp` entirely hand-registered indefinitely | None | Low, but the 12,965-line file remains the largest single maintenance and merge-conflict surface in the codebase, and every upstream option addition must be manually re-applied | Reject as the permanent state; acceptable as the fallback if the pilot fails its gates |
| Shadow-generate one low-risk family, verified against C2's oracle, keep the rest hand-registered | ~1 family initially, more only if proven | Low — the pilot runs alongside the existing code with a byte-for-byte gate before anything is deleted, and only touches Class 2 surface | **Adopt** |
| Auto-generate from an external schema definition language (e.g. JSON Schema, protobuf) requiring a new toolchain dependency | Complete, eventually | High — adds a build-time dependency and a second format to keep synchronized with `ConfigOptionDef`'s C++-specific fields (`ratio_over`, `enum_keys_map`, ordinal-sensitive serialization) for no evidence-backed benefit over a repo-local generator | Reject — no concrete need established yet |

## Gates

| Dimension | Required gate |
| --- | --- |
| Oracle match | Generated family's `normalized_option()` output is byte-identical to the hand-written family's, including `serialization_ordinal` |
| Scope | Pilot family is Class 2 only; no option with G-code-emission consequence is included |
| Compatibility | Existing config/registry/preset/legacy-3MF fixtures (C2, P1) continue to pass unmodified |
| Reversibility | The generated path can be reverted to hand-registration by re-pasting the deleted blocks; no format or wire-level state depends on generation having happened |
| Build | Generation adds no new required external toolchain; a config-only edit does not trigger a GUI rebuild, consistent with the Phase 4 `slic3r_config` boundary this pilot should not conflict with |

## Consequences

- Phase 3 is unblocked to close: this ADR is its stated exit gate, following C2 (registry oracle)
  and P1/rename (preset-mutation contract), both now Windows-verified.
- Phase 4 (`docs/architecture/module-boundaries.md`'s `slic3r_config` pilot, U1 Plater
  state-machine characterization) remains a separate, unblocked track — this ADR does not gate it
  and this ADR is not gated by it. They compose: a future `slic3r_config` target should have no
  reason to reject a shadow-generated definition as long as it still resolves to the same
  `ConfigOptionDef` construction the hand-written path produces today.
- No option is moved to a generated definition by this ADR alone. It authorizes the pilot
  (Workstream C4) and defines its gate; C4 remains separately estimated at 3-5 engineer-weeks and
  is not started by this document.
- If the pilot family fails its oracle-match gate, the correct outcome is reverting to hand
  registration for that family and recording why, per this plan's stop conditions — not loosening
  the gate to force a pass.

## Action items

1. [ ] Select the specific pilot family (Workstream C4) using the criteria in Decision step 1;
       record the chosen options and why they qualify.
2. [ ] Author the family's data source and generator.
3. [ ] Run the generated definitions alongside the hand-written ones and diff against C2's oracle.
4. [ ] On an exact match, delete the pilot family's hand-written blocks and re-run the full
       `[Config]`/`[Registry]`/`[Preset]` selectors plus the 67-vendor profile validator.
5. [ ] Record the outcome — adopted or reverted — in this plan's execution checkpoint log before
       considering a second pilot family.
